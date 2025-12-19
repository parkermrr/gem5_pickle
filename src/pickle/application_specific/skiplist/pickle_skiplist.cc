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

#include "pickle/application_specific/skiplist/pickle_skiplist.hh"

#include <stdint.h>

#include <cstdlib>
#include <ctime>
#include <iostream>

#include "base/trace.hh"
#include "debug/PickleSkipList.hh"

namespace gem5
{
    #define cmd_skiplist_init_level  0
    #define cmd_skiplist_init_key 1
    #define cmd_skiplist_insert  2
    #define cmd_skiplist_read  3

//Key Chunk Function Definitions

PickleSkipList::KeyChunk::KeyChunk()
{
        nextChunkAddr = 0;
        for (int i = 0; i < KEY_CHUNK_SIZE; i++)
        {
                dataPointers[i] = 0;
                for (int j = 0; j < KEY_SIZE; j++)
                {
                        keyBytes[i][j] = 0;
                }
        }
}

Addr PickleSkipList::KeyChunk::Create(PickleSkipList* list)
{
        Addr ret = list->keyChunkFreeList;
        list->keyChunkFree--;
        list->keyChunkFreeList += 1600;
        return ret;
}

void PickleSkipList::KeyChunk::Load(PickleSkipList* list, Addr loc, int line)
{
        if (line == -1)
        {
                for (int i = 0; i < 25; i++)
                {
                        std::tuple<unsigned char*, int> loadRequest = std::make_tuple(reinterpret_cast<unsigned char*>(this), i);
                        uint64_t loadAddr = loc + (i * 64);
                        list->inFlightLoads[loadAddr] = loadRequest;
                        list->numLoads++;
                        list->owner->makeLoadRequest(loadAddr);
                }
        }
        else
        {
                std::tuple<unsigned char*, int> loadRequest = std::make_tuple(reinterpret_cast<unsigned char*>(this), line);
                uint64_t loadAddr = loc + (line * 64);
                list->inFlightLoads[loadAddr] = loadRequest;
                list->numLoads++;
                list->owner->makeLoadRequest(loadAddr);
        }
}

void PickleSkipList::KeyChunk::Write(PickleSkipList* list, Addr loc, int line)
{
        if (line == -1)
        {
                for (int i = 0; i < 25; i++)
                {
                        uint64_t storeAddr = loc + (i * 64);
                        unsigned char* storeData = reinterpret_cast<unsigned char*>(this);
                        storeData += i * 64;

                        std::unique_ptr<uint8_t[]> bytes(new uint8_t[64]);
                        std::memcpy(bytes.get(), (uint8_t*)storeData, 64);
                        auto [r,rp] = list->owner->makeStoreRequest(storeAddr, std::move(bytes));
                }
        }
        else
        {
                uint64_t storeAddr = loc + (line * 64);
                unsigned char* storeData = reinterpret_cast<unsigned char*>(this);
                storeData += line * 64;

                std::unique_ptr<uint8_t[]> bytes(new uint8_t[64]);
                std::memcpy(bytes.get(), (uint8_t*)storeData, 64);
                auto [r,rp] = list->owner->makeStoreRequest(storeAddr, std::move(bytes));
        }
}

bool PickleSkipList::KeyChunk::IsFull()
{
    for (int i = 0; i < KEY_SIZE; i++)
    {
        if (keyBytes[KEY_CHUNK_SIZE - 1][i] != 0)
        {
            return true;
        }
    }
    return false;
}

// std::string PickleSkipList::KeyChunk::ToString()
// {
// 	std::ostringstream out;
// 	out << std::hex;
// 	for (int i = 0; i < KEY_CHUNK_SIZE; i++) //Search key chunk one byte of comparison at a time
// 	{
// 		for (int j = 0; j < KEY_SIZE; j++)
// 		{
// 			out << "Ox" << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(keyBytes[i][j]) << ", ";
// 		}
// 		out << std::endl;
// 	}
// 	return out.str();
// }

//Level Chunk Function Definitions

PickleSkipList::LevelChunk::LevelChunk()
{
        for (int i = 0; i < NUM_LEVELS; i++)
        {
                nextLevels[i] = 0;
        }

        for (int i = 0; i < KEY_SIZE; i++)
        {
                key[i] = 0;
        }

        data = 0;
        keyHeadAddr = 0;
}

void PickleSkipList::LevelChunk::setKey(unsigned char* newKey)
{
    for (int i = 0; i < KEY_SIZE; i++)
    {
        key[i] = newKey[i];
    }
}

Addr PickleSkipList::LevelChunk::Create(PickleSkipList* list)
{
        Addr ret = list->levelChunkFreeList;
        list->levelChunkFree--;
        list->levelChunkFreeList += 192;
        return ret;
}

void PickleSkipList::LevelChunk::Load(PickleSkipList* list, Addr loc, int line)
{
        if (line == -1)
        {
                for (int i = 0; i < 3; i++)
                {
                        std::tuple<unsigned char*, int> loadRequest = std::make_tuple(reinterpret_cast<unsigned char*>(this), i);
                        uint64_t loadAddr = loc + (i * 64);
                        list->inFlightLoads[loadAddr] = loadRequest;
                        list->numLoads++;
                        list->owner->makeLoadRequest(loadAddr);
                }
        }
        else
        {
                std::tuple<unsigned char*, int> loadRequest = std::make_tuple(reinterpret_cast<unsigned char*>(this), line);
                uint64_t loadAddr = loc + (line * 64);
                list->inFlightLoads[loadAddr] = loadRequest;
                list->numLoads++;
                list->owner->makeLoadRequest(loadAddr);
        }
}

void PickleSkipList::LevelChunk::Write(PickleSkipList* list, Addr loc, int line)
{
        if (line == -1)
        {
                for (int i = 0; i < 3; i++)
                {
                        uint64_t storeAddr = loc + (i * 64);
                        unsigned char* storeData = reinterpret_cast<unsigned char*>(this);
                        storeData += i * 64;

                        std::unique_ptr<uint8_t[]> bytes(new uint8_t[64]);
                        std::memcpy(bytes.get(), (uint8_t*)storeData, 64);
                        auto [r,rp] = list->owner->makeStoreRequest(storeAddr, std::move(bytes));
                }
        }
        else
        {
                uint64_t storeAddr = loc + (line * 64);
                unsigned char* storeData = reinterpret_cast<unsigned char*>(this);
                storeData += line * 64;

                std::unique_ptr<uint8_t[]> bytes(new uint8_t[64]);
                std::memcpy(bytes.get(), (uint8_t*)storeData, 64);
                auto [r,rp] = list->owner->makeStoreRequest(storeAddr, std::move(bytes));
        }

}

// std::string PickleSkipList::LevelChunk::ToString()
// {
// 	std::ostringstream out;
// 	for (int i = NUM_LEVELS - 1; i >= 0; i--)
// 	{
// 		if (nextLevels[i] != nullptr)
// 		{
// 			out << "Level " << i << " points to key 0x";
// 			for (int j = 0; j < KEY_SIZE; j++)
// 			{
// 				out << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(nextLevels[i][j]) << ' ';
// 			}
// 			out << std::endl;
// 		}

// 	}

// 	out << "Level Chunk Key: " << std::endl;

// 	for (int i = 0; i < KEY_SIZE; i++)
// 	{
// 		out << "0x" << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(key[i]) << " ";
// 	}

// 	out << std::endl;
// 	KeyChunk* currentKeyChunk = keyHead;

// 	while (currentKeyChunk != nullptr)
// 	{
// 		out << "|" << std::endl << "V" << std::endl;
// 		out << currentKeyChunk->ToString();
// 		out << std::endl;
// 		currentKeyChunk = currentKeyChunk->nextChunk;
// 	}

// 	return out.str();
// }

//SkipList Function Definitions

PickleSkipList::PickleSkipList(
    const PickleSkipListParams &params
) : SimObject(params),
    owner(params.owner)
{
}

PickleSkipList::~PickleSkipList()
{
}

void PickleSkipList::setOwner(PickleDevice* engine)
{
    this->owner = engine;
}

void PickleSkipList::clockTick()
{
        if (this->numLoads != 0)
        {
                return;
        }

        switch(this->skipListState)
        {
        case SkipListState::IDLE:
                return;
        case SkipListState::INSERT:
                if (this->insertRequestState == InsertRequestState::IDLE)
                {
                        this->insertRequestState = InsertRequestState::BEGIN;
                        memcpy(this->insertJob->key, &this->insertJob->readLine[this->insertJob->readOffset], 16);
                        DPRINTF(PickleSkipList, "Inserting key: %s\n", GetKey(reinterpret_cast<unsigned char*>(this->insertJob->key)).c_str());
                }
                Insert();
                if (this->insertRequestState == InsertRequestState::DONE)
                {
                        delete this->insertJob;
                        this->insertRequestState = InsertRequestState::IDLE;
                        this->skipListState = SkipListState::IDLE;
                        owner->setResponse(0, this->taskID);
                }
                return;
        case SkipListState::GET:
                if (this->getRequestState == GetRequestState::IDLE)
                {
                        this->getRequestState = GetRequestState::BEGIN;
                        memcpy(this->getJob->key, &this->getJob->readLine[this->getJob->readOffset], 16);
                        DPRINTF(PickleSkipList, "Searching for key: %s", GetKey(reinterpret_cast<unsigned char*>(this->getJob->key)).c_str());
                }
                Get();
                if (this->getRequestState == GetRequestState::DONE)
                {
                        Addr result = this->getJob->foundItem;
                        delete this->getJob;
                DPRINTF(PickleSkipList, "Result: %llx\n", result);
                        this->getRequestState = GetRequestState::IDLE;
                        this->skipListState = SkipListState::IDLE;
                owner->setResponse(0, this->taskID); //THIS IS SET TO 0 FOR DEBUG PURPOSES ONLY, SHOULD RETURN result
                }
                return;
        }
}

bool PickleSkipList::commandReceived(const uint64_t &command, uint8_t id)
{
    uint8_t cmd = command & 0x03;
    uint64_t data = command>>2;

    DPRINTF(
        PickleSkipList,
        "received command 0x%llx, id=%d, data=0x%llx, cmd = %d\n",
        command,id,data,cmd);


    if (cmd == cmd_skiplist_init_level)
    {
        //DPRINTF(PickleSkipList,"Received addr for initializing level free list\n");
        uint8_t* tempPtr = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(data));
        Addr baseAddr = (Addr) tempPtr;
        this->levelChunkFreeList = baseAddr;
        return true;
    }
    else if (cmd == cmd_skiplist_init_key)
    {
        //DPRINTF(PickleSkipList,"Received addr for initializing key free list\n");
        uint8_t* tempPtr = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(data));
        Addr baseAddr = (Addr) tempPtr;
        this->keyChunkFreeList = baseAddr;
        Initialize();
        return true;
    }
    else if (cmd == cmd_skiplist_insert)
    {
                DPRINTF(PickleSkipList,"INSERT Called\n");
                this->skipListState = SkipListState::INSERT;
                uint8_t* tempPtr = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(data));
                Addr addr = (Addr) tempPtr;
                Addr addrAligned = addr & ~0x3Fll;

                this->insertJob = new InsertJob;
                this->insertJob->readOffset = addr & 0x3Fll;
                this->insertJob->data = addr;
                this->insertJob->levels = -1;
                this->taskID = id;

                std::tuple<unsigned char*, int> loadRequest = std::make_tuple(this->insertJob->readLine, 0);
                this->inFlightLoads[addrAligned] = loadRequest;
                this->numLoads++;
                DPRINTF(PickleSkipList, "Loading Key located in cache line 0x%llx, original address 0x%llx\n", addrAligned, addr);
                this->owner->makeLoadRequest(addrAligned);
                return false;
    }
    else if (cmd == cmd_skiplist_read)
    {
                DPRINTF(PickleSkipList, "GET called\n");
                this->skipListState = SkipListState::GET;
                uint8_t* tempPtr = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(data));
                Addr addr = (Addr) tempPtr;
                Addr addrAligned = addr & ~0x3Fll;

                this->getJob = new GetJob;
                this->getJob->readOffset = addr & 0x3Fll;
                this->taskID = id;

                std::tuple<unsigned char*, int> loadRequest = std::make_tuple(this->getJob->readLine, 0);
                this->inFlightLoads[addrAligned] = loadRequest;
                this->numLoads++;
                DPRINTF(PickleSkipList, "Loading Key located in cache line 0x%llx, original address 0x%llx\n", addrAligned, addr);
                this->owner->makeLoadRequest(addrAligned);
                return false;
    }
    else {
        printf("ERROR: received an unknown command.");
        assert(0);
        return true;
    }
}

void PickleSkipList::receiveLoadResponse(const uint64_t& vaddr, std::unique_ptr<uint8_t[]> p)
{
        auto request = this->inFlightLoads[vaddr];
        unsigned char* objAddr = std::get<0>(request);
        objAddr += std::get<1>(request) * 64;

        //DPRINTF(PickleSkipList, "Received load response for 0x%llx, loading to object at 0x%llx, line position %d, final address 0x%llx\n", vaddr, (Addr) std::get<0>(request), std::get<1>(request), (Addr) objAddr);
        unsigned char* lineBytes = (unsigned char*) p.get();
        std::memcpy(objAddr, lineBytes, 64);
        this->inFlightLoads.erase(vaddr);

        this->numLoads--;
        clockTick();
}

void PickleSkipList::Initialize()
{
        LevelChunk* head = new LevelChunk;
        this->headAddr = head->Create(this);
        unsigned char minKey[KEY_SIZE] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        head->setKey(minKey);
        KeyChunk* headKeyChunk = new KeyChunk;
        head->keyHeadAddr = headKeyChunk->Create(this);

        LevelChunk* tail = new LevelChunk;
        unsigned char maxKey[KEY_SIZE] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        tail->setKey(maxKey);
        Addr tailAddr = tail->Create(this);
        for (int i = 0; i < NUM_LEVELS; i++)
        {
                head->nextLevels[i] = tailAddr;
        }
        KeyChunk* tailKeyChunk = new KeyChunk;
        tail->keyHeadAddr = tailKeyChunk->Create(this);

        head->Write(this, this->headAddr);
        headKeyChunk->Write(this, head->keyHeadAddr);
        tail->Write(this, tailAddr);
        tailKeyChunk->Write(this, tail->keyHeadAddr);

        //Cleaning up stored data on heap
        delete head;
        delete headKeyChunk;
        delete tail;
        delete tailKeyChunk;
}

void PickleSkipList::Insert()
{
        switch(this->insertRequestState)
        {
                case InsertRequestState::IDLE:
                {
                        return;
                }
                case InsertRequestState::BEGIN:
                {
                        if (this->insertJob->levels == -1)
                        {
                                unsigned char minKey[KEY_SIZE] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
                                if (EqualBytes(this->insertJob->key, minKey))
                                {
                                        this->insertJob->levels = 0;
                                }
                                else
                                {
                                        std::srand(static_cast<unsigned int>(std::time(0)));
                                        this->insertJob->levels = 1; //std::rand() % NUM_LEVELS;
                                }
                        }

                        this->findListState = FindListState::BEGIN;
                        this->findListJob = new FindListJob;
                        memcpy(this->findListJob->key, this->insertJob->key, sizeof(this->insertJob->key));
                        FindList();
                        this->insertRequestState = InsertRequestState::FINDLIST;
                        return;
                case InsertRequestState::FINDLIST:
                        FindList();
                        if (this->findListState == FindListState::DONE)
                        {
                                LevelChunk* foundLevelChunk = this->findListJob->foundList;
                                delete this->findListJob;
                                this->findListState = FindListState::IDLE;

                                this->insertJob->currentKeyChunkAddr = foundLevelChunk->keyHeadAddr;
                                this->insertJob->currentKeyChunk = new KeyChunk;
                                this->insertJob->lastKeyChunk = new KeyChunk;
                                this->insertJob->lastKeyChunkAddr = 0;

                                this->insertJob->currentKeyChunk->Load(this, this->insertJob->currentKeyChunkAddr);
                                this->insertRequestState = InsertRequestState::TRAVERSE;
                        }
                        return;
                }
                case InsertRequestState::TRAVERSE:
                {
                        DPRINTF(PickleSkipList, "InsertRequestState is TRAVERSE\n");
                        bool mask[KEY_CHUNK_SIZE];
                        for (int i = 0; i < KEY_CHUNK_SIZE; i++) //Initialize mask to all true
                        {
                                mask[i] = true;
                        }

                        for (int i = 0; i < KEY_SIZE - 8; i++) //Search key chunk one byte of comparison at a time
                        {
                                for (int j = 0; j < KEY_CHUNK_SIZE; j++)
                                {
                                        if (this->insertJob->currentKeyChunk->keyBytes[j][i] < this->insertJob->key[i] && this->insertJob->currentKeyChunk->keyBytes[j][i] != 0) //Comparing the ith byte of each key
                                        {
                                                mask[j] = false;
                                        }
                                }
                        }

                        int idx = -1;
                        for (int i = 0; i < KEY_CHUNK_SIZE; i++) //Check mask for equality
                        {
                                if (mask[i])
                                {
                                        idx = i;
                                        this->insertInChunkState = InsertInChunkState::BEGIN;
                                        this->insertInChunkJob = new InsertInChunkJob;
                                        memcpy(this->insertInChunkJob->key, this->insertJob->key, sizeof(this->insertJob->key));
                                        this->insertInChunkJob->data = this->insertJob->data;
                                        this->insertInChunkJob->levels = this->insertJob->levels;
                                        this->insertInChunkJob->keyChunk = this->insertJob->currentKeyChunk;
                                        this->insertInChunkJob->keyChunkAddr = this->insertJob->currentKeyChunkAddr;
                                        this->insertInChunkJob->idx = idx;
                                        InsertInChunk();
                                        delete this->insertJob->lastKeyChunk;
                                        this->insertRequestState = InsertRequestState::INSERTINCHUNK;
                                        return;
                                }
                        }

                        *this->insertJob->lastKeyChunk = *this->insertJob->currentKeyChunk;
                        this->insertJob->lastKeyChunkAddr = this->insertJob->currentKeyChunkAddr;
                        this->insertJob->currentKeyChunkAddr = this->insertJob->currentKeyChunk->nextChunkAddr;

                        if (this->insertJob->currentKeyChunkAddr == 0)
                        {
                                this->insertInChunkState = InsertInChunkState::BEGIN;
                                this->insertInChunkJob = new InsertInChunkJob;
                                memcpy(this->insertInChunkJob->key, this->insertJob->key, sizeof(this->insertJob->key));
                                this->insertInChunkJob->data = this->insertJob->data;
                                this->insertInChunkJob->levels = this->insertJob->levels;
                                this->insertInChunkJob->keyChunk = this->insertJob->lastKeyChunk;
                                this->insertInChunkJob->keyChunkAddr = this->insertJob->lastKeyChunkAddr;
                                this->insertInChunkJob->idx = -1;
                                InsertInChunk();
                                delete this->insertInChunkJob->currentLevelChunk;
                                this->insertRequestState = InsertRequestState::INSERTINCHUNK;
                                return;
                        }
                        else
                        {
                                this->insertJob->currentKeyChunk->Load(this, this->insertJob->currentKeyChunkAddr);
                                return;
                        }
                }
                case InsertRequestState::INSERTINCHUNK:
                {
                        DPRINTF(PickleSkipList, "InsertRequestState is INSERTINCHUNK\n");
                        InsertInChunk();
                        if (this->insertInChunkState == InsertInChunkState::DONE)
                        {
                                delete this->insertInChunkJob;
                                this->insertInChunkState = InsertInChunkState::IDLE;
                                this->insertRequestState = InsertRequestState::DONE;
                        }
                        return;
                }
        }
}

void PickleSkipList::InsertInChunk()
{
        switch(this->insertInChunkState)
        {
                case InsertInChunkState::IDLE:
                {
                        return;
                }
                case InsertInChunkState::BEGIN:
                {
                        if (this->insertInChunkJob->levels == 0)
                        {
                                if (this->insertInChunkJob->idx == -1) //Creating new chunk at end of list
                                {
                                        KeyChunk* newKeyChunk = new KeyChunk;
                                        Addr newKeyChunkAddr = newKeyChunk->Create(this);
                                        newKeyChunk->Insert(this->insertInChunkJob->key, this->insertInChunkJob->data, 0);
                                        newKeyChunk->nextChunkAddr = this->insertInChunkJob->keyChunk->nextChunkAddr;
                                        this->insertInChunkJob->keyChunk->nextChunkAddr = newKeyChunkAddr;
                                        this->insertInChunkJob->keyChunk->Write(this, this->insertInChunkJob->keyChunkAddr);
                                        newKeyChunk->Write(this, newKeyChunkAddr);
                                        delete newKeyChunk;
                                        delete this->insertInChunkJob->keyChunk;
                                }
                                else
                                {
                                        if (this->insertInChunkJob->keyChunk->IsFull()) //Need to push a key out to new chunk
                                        {
                                                KeyChunk* newKeyChunk = new KeyChunk;
                                                Addr newKeyChunkAddr = newKeyChunk->Create(this);
                                                newKeyChunk->Insert(this->insertInChunkJob->keyChunk->GetKey(KEY_CHUNK_SIZE - 1), this->insertInChunkJob->keyChunk->dataPointers[KEY_CHUNK_SIZE - 1], 0);
                                                newKeyChunk->nextChunkAddr = this->insertInChunkJob->keyChunk->nextChunkAddr;
                                                this->insertInChunkJob->keyChunk->nextChunkAddr = newKeyChunkAddr;
                                                newKeyChunk->Write(this, newKeyChunkAddr);
                                                delete newKeyChunk;
                                        }

                                        for (int i = KEY_CHUNK_SIZE - 1; i > this->insertInChunkJob->idx; i--)
                                        {
                                                unsigned char* moveKey = this->insertInChunkJob->keyChunk->GetKey(i - 1);
                                                this->insertInChunkJob->keyChunk->Insert(moveKey, this->insertInChunkJob->keyChunk->dataPointers[i - 1], i);
                                        }
                                        this->insertInChunkJob->keyChunk->Insert(this->insertInChunkJob->key, this->insertInChunkJob->data, this->insertInChunkJob->idx);
                                        this->insertInChunkJob->keyChunk->Write(this, this->insertInChunkJob->keyChunkAddr);
                                        delete this->insertInChunkJob->keyChunk;
                                }

                                this->insertInChunkState = InsertInChunkState::DONE;
                                return;
                        }
                        else
                        {
                                this->insertInChunkJob->newLevelChunk = new LevelChunk;
                                this->insertInChunkJob->newLevelChunkAddr = this->insertInChunkJob->newLevelChunk->Create(this);
                                this->insertInChunkJob->newLevelChunk->setKey(this->insertInChunkJob->key);
                                this->insertInChunkJob->newLevelChunk->data = this->insertInChunkJob->data;
                                KeyChunk* newKeyChunk = new KeyChunk;
                                Addr newKeyChunkAddr = newKeyChunk->Create(this);
                                this->insertInChunkJob->newLevelChunk->keyHeadAddr = newKeyChunkAddr;

                                if (this->insertInChunkJob->idx != -1)
                                {
                                        int newIndex = 0;
                                        for (int i = this->insertInChunkJob->idx; i < KEY_CHUNK_SIZE; i++)
                                        {
                                                unsigned char* moveKey = this->insertInChunkJob->keyChunk->GetKey(i);
                                                newKeyChunk->Insert(moveKey, this->insertInChunkJob->keyChunk->dataPointers[i], newIndex);
                                                this->insertInChunkJob->keyChunk->Remove(i);
                                                delete moveKey;
                                                newIndex++;
                                        }
                                        newKeyChunk->nextChunkAddr = this->insertInChunkJob->keyChunk->nextChunkAddr;
                                        this->insertInChunkJob->keyChunk->nextChunkAddr = 0;
                                        this->insertInChunkJob->keyChunk->Write(this, this->insertInChunkJob->keyChunkAddr);
                                }
                                newKeyChunk->Write(this, newKeyChunkAddr);
                                delete newKeyChunk;
                                delete this->insertInChunkJob->keyChunk;

                                Addr nextLevelAddr = this->headAddr;
                                this->insertInChunkJob->currentLevelChunk = new LevelChunk;
                                this->insertInChunkJob->loadedLevelChunks[nextLevelAddr] = this->insertInChunkJob->currentLevelChunk;
                                this->insertInChunkJob->currentLevelChunk->Load(this, nextLevelAddr);
                                this->insertInChunkState = InsertInChunkState::LOADLEVELS;
                                return;
                        }
                }
                case InsertInChunkState::LOADLEVELS:
                {
                        if (this->insertInChunkJob->currentLevelChunk->nextLevels[0] == 0 || GreaterBytes(this->insertInChunkJob->currentLevelChunk->key, this->insertInChunkJob->key))
                        {
                                this->insertInChunkState = InsertInChunkState::INSERTSPLICE;
                                return;
                        }
                        else
                        {
                                Addr nextLevelAddr = this->insertInChunkJob->currentLevelChunk->nextLevels[0];
                                this->insertInChunkJob->currentLevelChunk = new LevelChunk;
                                this->insertInChunkJob->loadedLevelChunks[nextLevelAddr] = this->insertInChunkJob->currentLevelChunk;
                                this->insertInChunkJob->currentLevelChunk->Load(this, nextLevelAddr);
                                return;
                        }
                }
                case InsertInChunkState::INSERTSPLICE:
                {
                        Addr prevAddr[NUM_LEVELS];
                        LevelChunk* prevChunk[NUM_LEVELS];
                        for (int i = 0; i < NUM_LEVELS; i++)
                        {
                                prevAddr[i] = this->headAddr;
                                prevChunk[i] = this->insertInChunkJob->loadedLevelChunks[this->headAddr];
                        }

                        for (int i = 0; i < this->insertInChunkJob->levels; i++) //Locate Previous level chunk at all levels we are concerned with
                        {
                                Addr currentChunkAddr = this->headAddr;
                                LevelChunk* currentChunk = this->insertInChunkJob->loadedLevelChunks[currentChunkAddr];
                                while (true)
                                {
                                        Addr nextChunkAddr = currentChunk->nextLevels[i];
                                        LevelChunk* nextChunk = this->insertInChunkJob->loadedLevelChunks[nextChunkAddr];
                                        if (nextChunk == nullptr || GreaterBytes(nextChunk->key, this->insertInChunkJob->key))
                                        {
                                                break;
                                        }
                                        else
                                        {
                                                currentChunkAddr = nextChunkAddr;
                                                currentChunk = nextChunk;
                                        }
                                }
                                prevAddr[i] = currentChunkAddr;
                                prevChunk[i] = currentChunk;
                        }

                        for (int i = 0; i < this->insertInChunkJob->levels; i++) //Update links
                        {
                                this->insertInChunkJob->newLevelChunk->nextLevels[i] = prevChunk[i]->nextLevels[i];
                                prevChunk[i]->nextLevels[i] = this->insertInChunkJob->newLevelChunkAddr;
                        }

                        this->insertInChunkJob->newLevelChunk->Write(this, this->insertInChunkJob->newLevelChunkAddr);

                        for (int i = 0; i < NUM_LEVELS; i++)
                        {
                                prevChunk[i]->Write(this, prevAddr[i]);
                        }

                        delete this->insertInChunkJob->newLevelChunk;
                        for (auto i = this->insertInChunkJob->loadedLevelChunks.begin(); i != this->insertInChunkJob->loadedLevelChunks.end(); i++)
                        {
                        delete i->second;
                        }
                        this->insertInChunkState = InsertInChunkState::DONE;
                        return;
                }
        }
}

void PickleSkipList::KeyChunk::Insert(unsigned char* key, Addr data, int idx)
{
        for (int i = 0; i < KEY_SIZE; i++)
        {
                keyBytes[idx][i] = key[i];
        }

        dataPointers[idx] = data;
}

void PickleSkipList::KeyChunk::Remove(int idx)
{
        for (int i = 0; i < KEY_SIZE; i++)
        {
                keyBytes[idx][i] = 0;
        }

        dataPointers[idx] = 0;
}

unsigned char* PickleSkipList::KeyChunk::GetKey(int idx)
{
        unsigned char* returnKey = new unsigned char[KEY_SIZE];
        for (int i = 0; i < KEY_SIZE; i++)
        {
                returnKey[i] = keyBytes[idx][i];
        }
        return returnKey;
}

bool PickleSkipList::Contains(unsigned char* key)
{
        (void)key;
        return false;
}

void PickleSkipList::Get()
{
        switch(this->getRequestState)
        {
        case GetRequestState::IDLE:
                return;
        case GetRequestState::BEGIN:
                this->findListState = FindListState::BEGIN;
                this->findListJob = new FindListJob;
                memcpy(this->findListJob->key, this->getJob->key, sizeof(this->getJob->key));
                FindList();
                this->getRequestState = GetRequestState::FINDLIST;
                return;
        case GetRequestState::FINDLIST:
                FindList();
                if (this->findListState == FindListState::DONE)
                {
                        unsigned char minKey[KEY_SIZE] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
                        this->getJob->foundLevelChunk = this->findListJob->foundList;
                        delete this->findListJob;
                        this->findListState = FindListState::IDLE;

                        if (EqualBytes(this->getJob->foundLevelChunk->key, this->getJob->key) && !EqualBytes(this->getJob->foundLevelChunk->key, minKey)) //First check key in the level chunk, ignore if it's the head list because it will have the minimum dummy key first
                        {
                                this->getJob->foundItem = this->getJob->foundLevelChunk->data;
                                delete this->getJob->foundLevelChunk;
                                delete this->findListJob;
                                this->getRequestState = GetRequestState::DONE;
                                return;
                        }

                        this->getJob->currentKeyChunk = new KeyChunk;
                        this->getJob->currentKeyChunkAddr = this->getJob->foundLevelChunk->keyHeadAddr;
                        this->getJob->currentKeyChunk->Load(this, this->getJob->currentKeyChunkAddr);

                        this->getRequestState = GetRequestState::TRAVERSE;
                }
                return;
        case GetRequestState::TRAVERSE:
                bool mask[KEY_CHUNK_SIZE];
                for (int i = 0; i < KEY_CHUNK_SIZE; i++) //Initialize mask to all true
                {
                        mask[i] = true;
                }

                for (int i = 0; i < KEY_SIZE - 8; i++) //Search key chunk one byte of comparison at a time
                {
                        for (int j = 0; j < KEY_CHUNK_SIZE; j++)
                        {
                                if (this->getJob->key[i] != this->getJob->currentKeyChunk->keyBytes[j][i]) //Comparing the ith byte of each key
                                {
                                        mask[j] = false;
                                }
                        }
                }

                for (int i = 0; i < KEY_CHUNK_SIZE; i++) //Check mask for equality
                {
                        if (mask[i]) //If we found the key
                        {
                                this->getJob->foundItem = this->getJob->currentKeyChunk->dataPointers[i];
                                delete this->getJob->currentKeyChunk;
                                this->getRequestState = GetRequestState::DONE;
                                return;
                        }
                }

                this->getJob->currentKeyChunkAddr = this->getJob->currentKeyChunk->nextChunkAddr; //Check next key chunk address

                if (this->getJob->currentKeyChunkAddr == 0) //Stop traversing
                {
                        Addr nextLevelAddr = this->getJob->foundLevelChunk->nextLevels[0];
                        delete this->getJob->foundLevelChunk;
                        delete this->getJob->currentKeyChunk;
                        this->getJob->nextLevelChunk = new LevelChunk;
                        this->getJob->nextLevelChunk->Load(this, nextLevelAddr);
                        this->getRequestState = GetRequestState::DONE; //THIS SHOUlD HEAD TO NEXTLEVELCHECK, TEMPORARY SWITCH FOR DEBUG
                        return;
                }

                this->getJob->currentKeyChunk->Load(this, this->getJob->currentKeyChunkAddr);   //Head to next chunk in list
                return;
        case GetRequestState::NEXTLEVELCHECK:
                if (EqualBytes(this->getJob->nextLevelChunk->key, this->getJob->key)) //Due to the nature of sequence numbers, we need to check here for next key
                {
                        this->getJob->foundItem = this->getJob->nextLevelChunk->data;
                        delete this->getJob->nextLevelChunk;
                        this->getRequestState = GetRequestState::DONE;
                        return;
                }
                else
                {
                        this->getJob->foundItem = 0;
                        delete this->getJob->nextLevelChunk;
                        this->getRequestState = GetRequestState::DONE;
                        return;
                }
        }
}

void PickleSkipList::FindList()
{
        switch(this->findListState)
        {
        case FindListState::IDLE:
                return;
        case FindListState::BEGIN:
                DPRINTF(PickleSkipList, "FindListState is BEGIN\n");
                this->findListJob->currentChunkAddr = this->headAddr;
                this->findListJob->currentChunk = new LevelChunk;

                DPRINTF(PickleSkipList, "Loading head of skiplist\n");
                DPRINTF(PickleSkipList, "Head located at 0x%llx, loading to object at 0x%llx\n", this->findListJob->currentChunkAddr, this->findListJob->currentChunk);
                this->findListJob->currentChunk->Load(this, this->findListJob->currentChunkAddr);

                this->findListState = FindListState::GATHER;
                return;
        case FindListState::GATHER:
                DPRINTF(PickleSkipList, "FindListState is GATHER\n");
                DPRINTF(PickleSkipList, "Issuing scattered loads for level chunks\n");
                for (int i = 0; i < NUM_LEVELS; i++) //Gather Load
                {
                        if (this->findListJob->currentChunk->nextLevels[i] != 0)
                        {
                                if (this->findListJob->nextLevelPartials.find(this->findListJob->currentChunk->nextLevels[i]) == this->findListJob->nextLevelPartials.end())
                                {
                                        DPRINTF(PickleSkipList, "Loading next partial chunk at level %d, address 0x%llx\n", i, this->findListJob->currentChunk->nextLevels[i]);
                                        LevelChunk* nextLevelPartial = new LevelChunk;
                                        this->findListJob->nextLevelPartials[this->findListJob->currentChunk->nextLevels[i]] = nextLevelPartial;
                                        nextLevelPartial->Load(this, this->findListJob->currentChunk->nextLevels[i], 0);
                                }
                                else
                                {
                                        DPRINTF(PickleSkipList, "Next chunk at level %d (address 0x%llx) previously loaded, skipping\n", i, this->findListJob->currentChunk->nextLevels[i]);
                                }
                        }
                        else
                        {
                                DPRINTF(PickleSkipList, "Next chunk at level %d is null\n", i);
                        }
                }

                this->findListState = FindListState::TRAVERSE;
                return;
        case FindListState::TRAVERSE:
                DPRINTF(PickleSkipList, "FindListState is TRAVERSE\n");
                uint8_t minKeys[NUM_LEVELS][KEY_SIZE];

                for (int i = 0; i < NUM_LEVELS; i++) //Gather Read
                {
                        if (this->findListJob->currentChunk->nextLevels[i] != 0)
                        {
                                LevelChunk* nextLevelPartial = this->findListJob->nextLevelPartials[this->findListJob->currentChunk->nextLevels[i]];

                                memcpy(minKeys[i], nextLevelPartial->key, KEY_SIZE);
                        }
                }

                for (auto i = this->findListJob->nextLevelPartials.begin(); i != this->findListJob->nextLevelPartials.end(); i++)
                {
                delete i->second;
                }
                this->findListJob->nextLevelPartials.clear();
                DPRINTF(PickleSkipList, "Beginning comparison of search key with gathered minimum keys...\n");
                DPRINTF(PickleSkipList, "Search key: %s\n", GetKey(reinterpret_cast<unsigned char*>(this->findListJob->key)).c_str());
                bool mask[NUM_LEVELS];
                for (int i = 0; i < NUM_LEVELS; i++) //'SIMD' Comparison
                {

                        if (this->findListJob->currentChunk->nextLevels[i] != 0)
                        {
                                DPRINTF(PickleSkipList, "Comparison key at level %d: %s\n", i, GetKey(reinterpret_cast<unsigned char*>(minKeys[i])).c_str());
                                mask[i] = PickleSkipList::GreaterEqualBytes(this->findListJob->key, minKeys[i]);
                                DPRINTF(PickleSkipList, "Result: %d\n", mask[i]);
                        }
                        else
                        {
                                mask[i] = 0;
                                DPRINTF(PickleSkipList, "Comparison key at level %d: Null Pointer\n", i);
                                DPRINTF(PickleSkipList, "Result: 0\n");
                        }

                }

                int idx = -1;
                for (int i = 0; i < NUM_LEVELS; i++) //Get index of furthest
                {
                        if (mask[i])
                        {
                                idx = i;
                        }
                }

                DPRINTF(PickleSkipList, "Index of furthest 1 in mask: %d\n", idx);

                if (idx != -1)
                {
                        //POTENTIAL OPTIMIZATION: MOVE CLEARING LOGIC FOR NEXTLEVELPARTIALS DOWN HERE AND EXCLUDE THE NEXT LEVEL CHUNK TO AVOID RELOADING INITIAL CACHE LINE
                        this->findListJob->currentChunkAddr = this->findListJob->currentChunk->nextLevels[idx];
                        this->findListJob->currentChunk->Load(this, this->findListJob->currentChunkAddr);
                        this->findListState = FindListState::GATHER;
                        return;
                }
                else
                {
                        this->findListState = FindListState::DONE;
                        this->findListJob->foundList = this->findListJob->currentChunk;
                        DPRINTF(PickleSkipList, "Key from found level chunk: %s\n", GetKey(reinterpret_cast<unsigned char*>(this->findListJob->currentChunk->key)).c_str());
                        return;
                }
        }
}

// std::string PickleSkipList::ToString()
// {
// 	std::ostringstream out;
// 	out << std::hex;
// 	LevelChunk* currentChunk = head;
// 	while (currentChunk != nullptr)
// 	{
// 		out << currentChunk->ToString();
// 		out << std::endl << "**********************" << std::endl;
// 		currentChunk = reinterpret_cast<PickleSkipList::LevelChunk*>(currentChunk->nextLevels[0]);
// 	}
// 	return out.str();
// }

std::string PickleSkipList::GetKey(unsigned char* key)
{
        std::ostringstream out;
        for (int j = 0; j < KEY_SIZE; j++)
        {
                out << "0x" << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(key[j]) << ' ';
        }
        return out.str();
}

uint64_t PickleSkipList::DecodeFixed64(const unsigned char* ptr)
{
        // Load the raw bytes
        uint64_t result;
        memcpy(&result, ptr, sizeof(result));  // gcc optimizes this to a plain load
        return result;
}

bool PickleSkipList::GreaterEqualBytes(unsigned char* a, unsigned char* b) // returns true iff a >= b
{
        for (int i = KEY_SIZE - 9; i >= 0; i--)
        {
                if (a[i] < b[i])
                {
                        return false;
                }
                else if (a[i] > b[i])
                {
                        return true;
                }
        }

        //If we got here, the keys match, use the tag as the tie break
        for (int i = KEY_SIZE - 1; i > KEY_SIZE - 8; i--)
        {
                if (a[i] > b[i])
                {
                        return false;
                }
                else if (a[i] < b[i])
                {
                        return true;
                }
        }
        return true;
}

bool PickleSkipList::GreaterBytes(unsigned char* a, unsigned char* b) // returns true iff a > b
{
        for (int i = KEY_SIZE - 8; i >= 0; i--)
        {
                if (a[i] < b[i])
                {
                        return false;
                }
                else if (a[i] > b[i])
                {
                        return true;
                }
        }

        //If we got here, the keys match, use the tag as the tie break
        for (int i = KEY_SIZE - 1; i > KEY_SIZE - 8; i--)
        {
                if (a[i] > b[i])
                {
                        return false;
                }
                else if (a[i] < b[i])
                {
                        return true;
                }
        }
        return false;
}

bool PickleSkipList::EqualBytes(unsigned char* a, unsigned char* b)
{
        for (int i = 0; i < KEY_SIZE - 8; i++)
        {
                if (a[i] != b[i])
                {
                        return false;
                }
        }
        //If we got here, the keys match
        return true;
}

bool PickleSkipList::IsZero(unsigned char* a)
{
        for (int i = 0; i < KEY_SIZE; i++)
        {
                if (a[i] != 0)
                {
                        return false;
                }
        }
        return true;
}

void PickleSkipList::SwapBytes(unsigned char* a, unsigned char* b)
{
        unsigned char swap;
        for (int i = 0; i < KEY_SIZE; i++)
        {
                swap = a[i];
                a[i] = b[i];
                b[i] = swap;
        }
}

}; // namespace gem5
