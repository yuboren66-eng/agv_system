#include "shm_manager.h"
#include "logger.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <thread>
#include <chrono>

int main(){
    LOG_DEBUG("OV-test","2Mapdata:%d,%d",sizeof(agv::MapData),alignof(agv::MapData));
    LOG_DEBUG("OV-test","2CarData:%d,%d",sizeof(agv::CarData),alignof(agv::CarData));
    LOG_DEBUG("OV-test","ShmHeader:%d,%d",sizeof(agv::ShmHeader),alignof(agv::ShmHeader));
    LOG_DEBUG("OV-test","map-Node:%d,%d",sizeof(agv::Node),alignof(agv::Node));
    LOG_DEBUG("OV-test","map-Edge:%d,%d",sizeof(agv::Edge),alignof(agv::Edge));
    LOG_DEBUG("OV-test","map-AdjEntry:%d,%d",sizeof(agv::AdjEntry),alignof(agv::AdjEntry));
    LOG_DEBUG("OV-test","Car:%d,%d",sizeof(agv::Car),alignof(agv::Car));
    LOG_DEBUG("OV-test","Seqlock:%d,%d",sizeof(agv::Seqlock),alignof(agv::Seqlock));
}
