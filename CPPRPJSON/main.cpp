
#pragma comment(lib, "CPPRP.lib")
#include "../CPPRP/ReplayFile.h"
#include "SerializeFunctions.h"
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <map>
#include <filesystem>
#include <queue>
#include <unordered_map>
#include <fstream>
#include "OptionsParser.h"
#undef max
static int a = CPPRP::JSON::Test();

struct UpdateData
{
	const CPPRP::ActorStateData asd;
	const std::vector<uint32_t> props;
};
void SerializeProp(CPPRP::JSON::Writer& writer, const std::string& name, std::shared_ptr<CPPRP::Property>& currentProperty, const bool specialByteProp)
{

	writer.String(name.c_str());
	switch (currentProperty->property_type[0])
	{
	case 'N':
	{
		if (currentProperty->property_type[1] == 'o') //Type is "None"
		{
			writer.Null();
		}
		else //Type is "Name"
		{
			writer.String(std::any_cast<std::string>(currentProperty->value).c_str());
		}
	}
	break;
	case 'I': //IntProperty
	{
		writer.Int(std::any_cast<int32_t>(currentProperty->value));
	}
	break;
	case 'S': //StringProperty
	{
		writer.String(std::any_cast<std::string>(currentProperty->value).c_str());
	}
	break;
	case 'B':
	{
		if (currentProperty->property_type[1] == 'y') //Type is "ByteProperty"
		{
			CPPRP::EnumProperty ep = std::any_cast<CPPRP::EnumProperty>(currentProperty->value);
			writer.StartObject();
			writer.String("Type");
			writer.String(ep.type.c_str());
			writer.String("Value");
			writer.String(ep.value.c_str());
			writer.EndObject();
		}
		else //Type is "BoolProperty", but unlike network data, is stored as entire byte
		{
			if (specialByteProp)
			{
				writer.Int(std::any_cast<uint32_t>(currentProperty->value));
			}
			else
			{
				writer.Int(std::any_cast<uint8_t>(currentProperty->value));
			}
			
		}
	}
	break;
	case 'Q': //QWordProperty
	{
		writer.Uint64(std::any_cast<uint64_t>(currentProperty->value));
	}
	break;
	case 'F': //FloatProperty
	{
		writer.Double(std::any_cast<float>(currentProperty->value));
	}
	break;
	case 'A': //ArrayProperty
	{
		auto temp = (std::any_cast<std::vector<std::unordered_map<std::string, std::shared_ptr<CPPRP::Property>>>>(currentProperty->value));
		
		writer.StartArray();
		for (auto& wot : temp)
		{
			writer.StartObject();
			for (auto& wot2 : wot)
			{
				SerializeProp(writer, wot2.first, wot2.second, specialByteProp);
			}
			writer.EndObject();
		}
		writer.EndArray();
	}
	break;
	default: //Die
		writer.Null();
		//assert(1 == 2);
		break;
	}


}

int main(int argc, char* argv[])
{
	OptionsParser op(argc, argv);
	std::string inputFile = op.looseOption;
	if (inputFile.size() == 0)
	{
		inputFile = op.GetStringValue({ "i", "input" });
	}
	if (inputFile.size() == 0)
	{
		std::cout << "No input file given! Pass path to replay file as default argument via -i or --input\n";
		return 0;
	}
	if (!std::filesystem::exists(inputFile))
	{
		std::cout << "Failed to open file " << inputFile << "\n";
		return 0;
	}

	auto replayFile = std::make_shared<CPPRP::ReplayFile>(inputFile);
	if (!replayFile->Load())
	{
		std::cout << "Cannot open file, it exists but cannot open? " << inputFile << "\n";
		return 0;
	}

	try
	{
		replayFile->DeserializeHeader();
	}
	catch (CPPRP::GeneralParseException<BitReaderType>& gpe)
	{
		std::cout << "DeserializeHeader threw exception: " << gpe.errorMsg << "\n";
		return 0;
	}
	replayFile->PreprocessTables();

	auto headerData = replayFile->replayFile;
	std::vector<CPPRP::ActorStateData> createdActorsThisTick;
	std::vector<CPPRP::ActorStateData> deletedActorsThisTick;
	std::vector<UpdateData> updatedActorsThisTick;

	rapidjson::StringBuffer s;
	CPPRP::JSON::Writer writer(s);
	writer.StartObject();
	writer.String("Replay");
	writer.StartObject();
	writer.String("Header");
	writer.StartObject();

	writer.String("EngineVersion");
	writer.Uint(headerData->header.engineVersion);
	writer.String("LicenseeVersion");
	writer.Uint(headerData->header.licenseeVersion);
	writer.String("NetVersion");
	writer.Uint(headerData->header.netVersion);
	writer.String("CRC");
	writer.Uint(headerData->header.crc);
	writer.String("Size");
	writer.Uint(headerData->header.size);
	writer.String("ReplayType");
	writer.String(headerData->replayType.c_str());

	writer.String("Properties");
	const bool useSpecialByteProp = headerData->header.engineVersion == 0 &&
		headerData->header.licenseeVersion == 0 &&
		headerData->header.netVersion == 0;
	writer.StartObject();
	for (auto prop : headerData->properties)
	{
		SerializeProp(writer, prop.first, prop.second, useSpecialByteProp);
	}
	writer.EndObject();

	writer.String("Keyframes");
	writer.StartArray();

	for (auto kv : headerData->keyframes)
	{
		writer.StartObject();
		writer.String("Time");
		writer.Double(kv.time);
		writer.String("Frame");
		writer.Uint(kv.frame);
		writer.String("Filepos");
		writer.Uint(kv.filepos);
		writer.EndObject();
	}
	writer.EndArray();

	writer.EndObject();
		
	writer.String("Body");
	//writer.StartObject();

	replayFile->createdCallbacks.push_back([&](const CPPRP::ActorStateData& asd)
		{
			createdActorsThisTick.push_back(asd);
		});
	replayFile->actorDeleteCallbacks.push_back([&](const CPPRP::ActorStateData& asd)
		{
			deletedActorsThisTick.push_back(asd);
		});

	replayFile->updatedCallbacks.push_back([&](const CPPRP::ActorStateData& asd, const std::vector<uint32_t>& props)
		{
			updatedActorsThisTick.push_back({ asd, props });
		});
		
	replayFile->tickables.push_back([&](const CPPRP::Frame frame, const std::unordered_map<int, CPPRP::ActorStateData>& actorStats)
		{
			writer.StartObject();
			writer.String("Frame");
			writer.Uint(frame.frameNumber);

			writer.String("Time");
			writer.Double(frame.time);

			writer.String("Delta");
			writer.Double(frame.delta);

			writer.String("Created");
			writer.StartArray();
			for (auto created : createdActorsThisTick)
			{
				writer.StartObject();
				writer.String("Id");
				writer.Uint(created.actorId);
					
				std::string name = replayFile->replayFile->names.at(created.nameId);
				std::string className = replayFile->replayFile->objects.at(created.classNameId);
				writer.String("Name");
				writer.String(name.c_str());

				writer.String("ClassName");
				writer.String(className.c_str());
				bool hasInitialPosition = replayFile->HasInitialPosition(className);
				writer.String("HasInitialPosition");
				writer.Bool(hasInitialPosition);
				if (hasInitialPosition)
				{
					writer.String("InitialPosition");
					CPPRP::JSON::Serialize<CPPRP::Vector3I>(writer, created.actorObject->Location);
				}

				bool hasInitialRotation = replayFile->HasRotation(className);
				writer.String("HasInitialRotation");
				writer.Bool(hasInitialRotation);
				if (hasInitialRotation)
				{
					writer.String("InitialRotation");
					CPPRP::JSON::Serialize<CPPRP::Rotator>(writer, created.actorObject->Rotation);
				}
				writer.EndObject();
			}
			writer.EndArray();

			writer.String("Deleted");
			writer.StartArray();
			for (auto deleted : deletedActorsThisTick)
			{
				writer.Uint(deleted.actorId);
			}
			writer.EndArray();
				

			writer.String("Updated");
			writer.StartArray();
			for (auto updated : updatedActorsThisTick)
			{
				writer.StartObject();
				writer.String("Id");
				writer.Uint(updated.asd.actorId);
				writer.String("UpdatedProperties");
				writer.StartArray();

				for (auto updatedProp : updated.props)
				{
					writer.StartObject();
						
					std::string objectName = replayFile->replayFile->objects[updatedProp];
					auto a = updated.asd.actorObject;
					CPPRP::JSON::writerFuncs[objectName](writer, a);
					writer.EndObject();
				}

				writer.EndArray();

				writer.EndObject();
			}
			writer.EndArray();
			writer.EndObject();
			createdActorsThisTick.clear();
			updatedActorsThisTick.clear();
			deletedActorsThisTick.clear();
		});
	writer.StartArray();
	try
	{
		if (!op.GetBoolValue({ "ho", "header" }, false))
		{
			replayFile->Parse();
		}
	}
	catch (const CPPRP::InvalidVersionException& e)
	{
		std::cout << "InvalidVersionException: " << e.what() << " on file " << inputFile << "\n";
		return 0;
	}
	catch (const CPPRP::PropertyDoesNotExistException& e)
	{
		std::cout << "PropertyDoesNotExistException: " << e.what() << " on file " << inputFile << "\n";
		return 0;
	}
	catch (const CPPRP::AttributeParseException<uint32_t>& e)
	{
		std::cout << "AttributeParseException: " << e.what() << " on file " << inputFile << "\n";
		return 0;
	}
	catch (const CPPRP::GeneralParseException<uint32_t>& e)
	{
		std::cout << "GeneralParseException: " << e.what() << " on file " << inputFile << "\n";
		return 0;
	}
	catch(const std::exception &e) //e
	{
		std::cout << "GeneralException: " << e.what() <<  "on file " << inputFile << "\n";
		return 0;
	}
	catch (...)
	{
		std::cout << "UnknownException: on file " << inputFile << "\n";
		return 0;
	}
	writer.EndArray();

	//writer.EndObject();
	writer.EndObject();
	writer.EndObject();

	std::string outJsonString = s.GetString();
	std::string outFile = op.GetStringValue({ "o", "output" });
	if (outFile.size() > 0)
	{
		std::ofstream outFileStream(outFile);
		outFileStream << outJsonString;
	}
	if (outFile.size() > 0 && op.GetBoolValue({ "stdout", "print" }, false) || outFile.size() == 0)
	{
		std::cout << outJsonString;
	}
}