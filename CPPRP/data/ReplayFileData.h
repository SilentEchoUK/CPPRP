#pragma once
#include <stdint.h>
#include <any>
#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <memory>

/*
This file contains all base types such as vectors and rotators (mostly classes that are read in the bitreader directly)
It also contains data structures for things that are stored in the header of a replay
*/

namespace CPPRP
{
	struct Vector3
	{
		float x{ 0 }, y{ 0 }, z{ 0 };

		std::string ToString() const
		{
			return std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z);
		}
	};

	struct Vector3I
	{
		int x{ 0 }, y{ 0 }, z{ 0 };

		explicit operator Vector3() const
		{
			return Vector3
			{
				static_cast<float>(x),
				static_cast<float>(y),
				static_cast<float>(z)
			};
		};

		std::string ToString() const
		{
			return std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z);
		}
	};

	struct Rotator
	{
		int pitch{ 0 }, yaw{ 0 }, roll{ 0 };

		std::string ToString() const
		{
			return std::to_string(pitch) + ", " + std::to_string(yaw) + ", " + std::to_string(roll);
		}
	};

	struct Quat
	{
		float  x{ 0 }, y{ 0 }, z{ 0 }, w{ 0 };

		std::string ToString() const
		{
			return std::to_string(w) + ", " + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z);
		}
	};

	enum Platform
	{
		Platform_Unknown = 0,
		Platform_Steam = 1,
		Platform_PS4 = 2,
		Platform_PS3 = 3,
		Platform_Dingo = 4,
		Platform_QQ = 5,
		Platform_Switch = 6, //Is in gamefiles as wegame..
		//Platform_NNX = 7,
		Platform_PsyNet = 7, //is 8 in gamefiles
		Platform_MAX = 9
	};

	struct UniqueId
	{
		uint8_t platform{ 0 };
		uint8_t playerNumber{ 0 };
		//uint64_t uniqueID{ 0 };

		std::string ToString() const
		{
			return "";// return std::to_string(platform) + "|" + std::to_string(uniqueID) + "|" + std::to_string(playerNumber);
		}
	};


	struct SteamID : public UniqueId
	{
		uint64_t steamID{ 0 };
	};

	struct XBoxID : public UniqueId
	{
		uint64_t xboxID{ 0 };
	};

	struct SwitchID : public UniqueId
	{
		uint64_t a{ 0 };
		uint64_t b{ 0 };
		uint64_t c{ 0 }; 
		uint64_t d{ 0 };
	};

	struct PS4ID : public UniqueId
	{
		uint64_t psId{ 0 };
	};

	struct PsyNetID : public UniqueId
	{
		uint64_t a{ 0 };
		uint64_t b{ 0 };
		uint64_t c{ 0 };
		uint64_t d{ 0 };
	};

	struct UnkownId : public UniqueId
	{
		uint32_t unknown{ 0 };
	};

	struct Property
	{
		std::string property_name{ "" };
		std::string property_type{ "" };
		std::any value;
	};

	struct FileHeader
	{
		uint32_t size{ 0 };
		uint32_t crc{ 0 };
		uint32_t engineVersion{ 0 };
		uint32_t licenseeVersion{ 0 };
		uint32_t netVersion{ 0 };
	};

	struct KeyFrame
	{
		float time{ 0.f };
		uint32_t frame{ 0 };
		uint32_t filepos{ 0 };
	};

	struct ReplayTick
	{
		std::string type{ "" };
		uint32_t frame{ 0 };
	};

	struct ClassIndex
	{
		std::string class_name{ "" };
		uint32_t index{ 0 };
	};

	struct PropIndexId
	{
		int32_t prop_index{ 0 };
		int32_t prop_id{ 0 };
	};

	struct ClassNet
	{
		int32_t index{ 0 };
		int32_t parent{ 0 };
		std::shared_ptr<ClassNet> parent_class{ nullptr };
		int32_t id{ 0 };
		int32_t prop_indexes_size{ 0 };
		std::vector<PropIndexId> prop_indexes;
		uint16_t max_prop_id{ 0 };
		std::vector<uint16_t> property_id_cache;
	};

	struct ActorState
	{
		std::shared_ptr<ClassNet> classNet{ nullptr };
		uint32_t actor_id{ 0 };
		uint32_t name_id{ 0 };
		bool alive{ false };
		std::unordered_map<std::string, std::shared_ptr<void>> fields;
		//Vector3 position;
		//Rotator rotation;
	};

	struct Frame
	{
		float time;
		float delta;
		size_t position;
		std::unordered_map<uint32_t, ActorState> states;
	};

	struct EnumProperty
	{
		std::string type;
		std::string value;
	};

	struct ReplayFileData
	{
		//All header props
		FileHeader header;
		std::string replayType;
		std::unordered_map<std::string, std::shared_ptr<Property>> properties;
		uint32_t body_size;
		uint32_t crc2;

		std::vector<std::string> levels;
		std::vector<KeyFrame> keyframes;

		uint32_t netstream_size;
		const char* netstream_data;

		std::vector<ReplayTick> replayticks;
		std::vector<std::string> replicated_packages;
		std::vector<std::string> objects;
		std::vector<std::string> names;
		std::vector<ClassIndex> class_indices;
		std::vector<std::shared_ptr<ClassNet>> classnets;
	};


}