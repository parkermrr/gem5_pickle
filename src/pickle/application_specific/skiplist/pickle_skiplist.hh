/*
 * Copyright (c) 2024 Advanced Micro Devices, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __CEREBELLUM_SKIPLIST_HH__
#define __CEREBELLUM_SKIPLIST_HH__
#define LOCAL_CACHE_LINE_SIZE 64
#define NUM_LEVELS 20
#define KEY_SIZE 16
#define KEY_CHUNK_SIZE 64

#include <vector>
#include <unordered_map>
#include <tuple>

#include "pickle/device/pickle_device.hh"
#include "params/PickleSkipList.hh"
#include "sim/sim_object.hh"
namespace gem5
{

enum class SkipListState
{
    IDLE = 0,
    INSERT = 2,
    GET = 3,
};

enum class InsertRequestState
{
	IDLE = 0,
    BEGIN = 1,
    FINDLIST = 2,
    TRAVERSE = 3,
    INSERTINCHUNK = 4,
    DONE = 5
};

enum class FindListState
{
	IDLE = 0,
    BEGIN = 1,
    GATHER = 2,
	TRAVERSE = 3,
    DONE = 4
};

enum class InsertInChunkState
{
	IDLE = 0,
	BEGIN = 1,
	LOADLEVELS = 2,
	INSERTSPLICE = 3,
    DONE = 4
};

enum class GetRequestState
{
	IDLE = 0,
    BEGIN = 1,
    FINDLIST = 2,
    TRAVERSE = 3,
	NEXTLEVELCHECK = 4,
    DONE = 5
};

class PickleSkipList: public SimObject
{
    public:
        class KeyChunk
        {
            public:
                unsigned char keyBytes[KEY_CHUNK_SIZE][KEY_SIZE];
                Addr dataPointers[KEY_CHUNK_SIZE];
                Addr nextChunkAddr;
                KeyChunk();
                void Insert(unsigned char* key, Addr data, int idx);
                void Remove(int idx);
                unsigned char* GetKey(int idx);
                bool IsFull();
                Addr Create(PickleSkipList* list);
                void Load(PickleSkipList* list, Addr loc, int line = -1);
                void Write(PickleSkipList* list, Addr loc, int line = -1);
    			// std::string ToString();
        };

        class LevelChunk
        {
            public:
                unsigned char key[KEY_SIZE];
                Addr data;
                Addr keyHeadAddr;
                Addr nextLevels[NUM_LEVELS];
                LevelChunk();
                void setKey(unsigned char* newKey);
    			Addr Create(PickleSkipList* list);
                void Load(PickleSkipList* list, Addr loc, int line = -1);
                void Write(PickleSkipList* list, Addr loc, int line = -1);
    			// std::string ToString();
        };

        class InsertJob
        {
            public:
				unsigned char readLine[64];
				Addr readOffset;
                unsigned char key[16];
                Addr data;
                int levels;
                KeyChunk* currentKeyChunk;
                Addr currentKeyChunkAddr;
                KeyChunk* lastKeyChunk;
                Addr lastKeyChunkAddr;
                int numLoads;
        };

        class GetJob
        {
            public:
				unsigned char readLine[64];
				Addr readOffset;
                unsigned char key[16];
                LevelChunk* foundLevelChunk;
                KeyChunk* currentKeyChunk;
                Addr currentKeyChunkAddr;
                LevelChunk* nextLevelChunk;
                int numLoads;
                Addr foundItem;
        };

        class FindListJob
        {
            public:
                unsigned char key[16];
                LevelChunk* currentChunk;
                Addr currentChunkAddr;
				std::unordered_map<Addr, LevelChunk*> nextLevelPartials;
                int numLoads;
                LevelChunk* foundList;
        };

        class InsertInChunkJob
        {
            public:
                unsigned char key[16];
                Addr data;
                KeyChunk* keyChunk;
                Addr keyChunkAddr;
                int levels;
                int idx;
                Addr newLevelChunkAddr;
                LevelChunk* newLevelChunk;
                LevelChunk* currentLevelChunk;
                std::unordered_map<Addr, LevelChunk*> loadedLevelChunks;
        };

    private:
    	SkipListState skipListState = SkipListState::IDLE;
    	InsertRequestState insertRequestState = InsertRequestState::IDLE;
    	GetRequestState getRequestState = GetRequestState::IDLE;
    	FindListState findListState = FindListState::IDLE;
    	InsertInChunkState insertInChunkState = InsertInChunkState::IDLE;

        InsertJob* insertJob;
        GetJob* getJob;
        FindListJob* findListJob;
        InsertInChunkJob* insertInChunkJob;

        PickleDevice* owner;
        std::unordered_map<Addr, std::tuple<unsigned char*, int>> inFlightLoads;
        Addr headAddr;
		Addr levelChunkFreeList;
		Addr keyChunkFreeList;
		int levelChunkFree = 1000;
		int keyChunkFree = 1000;
		uint8_t taskID;
		int numLoads = 0;
        PARAMS(PickleSkipList);

    public:
        PickleSkipList(const PickleSkipListParams &params);
        ~PickleSkipList();
        void setOwner(PickleDevice* owner);
		bool commandReceived(const uint64_t &command, uint8_t id);
		void receiveLoadResponse(const uint64_t& vaddr, std::unique_ptr<uint8_t[]> p);
		void clockTick();

		// the interface
		void Initialize();
        void Insert();
		bool Contains(unsigned char* key);
		void Get();
        std::string GetKey(unsigned char* key);
		// std::string ToString();


    private:
        void InsertInChunk();
        void FindList();
		uint64_t DecodeFixed64(const unsigned char* ptr);
        bool GreaterEqualBytes(unsigned char* a, unsigned char* b); // returns true iff a >= b
		bool GreaterBytes(unsigned char* a, unsigned char* b); // returns true iff a > b
		bool EqualBytes(unsigned char* a, unsigned char* b);
		bool IsZero(unsigned char* a);
		void SwapBytes(unsigned char* a, unsigned char* b);
};

}; // namespace gem5

#endif // __CEREBELLUM_SKIPLIST_HH__
