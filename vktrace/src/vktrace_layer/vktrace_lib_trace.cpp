/*
 * Vulkan
 *
 * Copyright (C) 2014 LunarG, Inc.
 * Copyright (C) 2015 Valve Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include <stdbool.h>
#include <unordered_map>
#include "vulkan.h"
#include "vktrace_platform.h"
#include "vk_dispatch_table_helper.h"
#include "vktrace_common.h"
#include "vktrace_lib_helpers.h"
#include "vktrace_vk_vk_ext_khr_swapchain.h"
#include "vktrace_vk_vk_ext_khr_device_swapchain.h"
#include "vk_ext_khr_device_swapchain_struct_size_helper.h"
#include "vktrace_vk_vk_debug_report_lunarg.h"
#include "vktrace_vk_vk_debug_marker_lunarg.h"
#include "vktrace_vk_vk.h"
#include "vktrace_interconnect.h"
#include "vktrace_filelike.h"
#include "vktrace_trace_packet_utils.h"
#include "vktrace_vk_exts.h"
#include <stdio.h>

// declared as extern in vktrace_lib_helpers.h
VKTRACE_CRITICAL_SECTION g_memInfoLock;
VKMemInfo g_memInfo = {0, NULL, NULL, 0};


std::unordered_map<void *, layer_device_data *> g_deviceDataMap;
std::unordered_map<void *, layer_instance_data *> g_instanceDataMap;


layer_instance_data *mid(void *object)
{
    dispatch_key key = get_dispatch_key(object);
    std::unordered_map<void *, layer_instance_data *>::const_iterator got;
    got = g_instanceDataMap.find(key);
    assert(got != g_instanceDataMap.end());
    return got->second;
}

layer_device_data *mdd(void* object)
{
    dispatch_key key = get_dispatch_key(object);
    std::unordered_map<void *, layer_device_data *>::const_iterator got;
    got = g_deviceDataMap.find(key);
    assert(got != g_deviceDataMap.end());
    return got->second;
}

static layer_instance_data *initInstanceData(std::unordered_map<void *, layer_instance_data *> &map, const VkBaseLayerObject *instancew)
{
    layer_instance_data *pTable;
    assert(instancew);
    dispatch_key key = get_dispatch_key(instancew->baseObject);

    std::unordered_map<void *, layer_instance_data *>::const_iterator it = map.find(key);
    if (it == map.end())
    {
        pTable =  new layer_instance_data();
        map[key] = pTable;
    } else
    {
        return it->second;
    }

    layer_init_instance_dispatch_table(&pTable->instTable, instancew);

    return pTable;
}

static layer_device_data *initDeviceData(std::unordered_map<void *, layer_device_data *> &map, const VkBaseLayerObject *devw)
{
    layer_device_data *pTable;
    assert(devw);
    dispatch_key key = get_dispatch_key(devw->baseObject);

    std::unordered_map<void *, layer_device_data *>::const_iterator it = map.find(key);
    if (it == map.end())
    {
        pTable =  new layer_device_data();
        map[key] = pTable;
    } else
    {
        return it->second;
    }

    layer_initialize_dispatch_table(&pTable->devTable, devw);

    return pTable;
}

VKTRACER_EXPORT VkResult VKAPI __HOOKED_vkAllocMemory(
    VkDevice device,
    const VkMemoryAllocInfo* pAllocInfo,
    VkDeviceMemory* pMem)
{
    vktrace_trace_packet_header* pHeader;
    VkResult result;
    packet_vkAllocMemory* pPacket = NULL;
    CREATE_TRACE_PACKET(vkAllocMemory, get_struct_chain_size((void*)pAllocInfo) + sizeof(VkDeviceMemory));
    result = mdd(device)->devTable.AllocMemory(device, pAllocInfo, pMem);
    vktrace_set_packet_entrypoint_end_time(pHeader);
    pPacket = interpret_body_as_vkAllocMemory(pHeader);
    pPacket->device = device;
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pAllocInfo), sizeof(VkMemoryAllocInfo), pAllocInfo);
    add_alloc_memory_to_trace_packet(pHeader, (void**)&(pPacket->pAllocInfo->pNext), pAllocInfo->pNext);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pMem), sizeof(VkDeviceMemory), pMem);
    pPacket->result = result;
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pAllocInfo));
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pMem));
    FINISH_TRACE_PACKET();
    // begin custom code
    add_new_handle_to_mem_info(*pMem, pAllocInfo->allocationSize, NULL);
    // end custom code
    return result;
}

VKTRACER_EXPORT VkResult VKAPI __HOOKED_vkMapMemory(
    VkDevice device,
    VkDeviceMemory mem,
    VkDeviceSize offset,
    VkDeviceSize size,
    VkFlags flags,
    void** ppData)
{
    vktrace_trace_packet_header* pHeader;
    VkResult result;
    packet_vkMapMemory* pPacket = NULL;
    CREATE_TRACE_PACKET(vkMapMemory, sizeof(void*));
    result = mdd(device)->devTable.MapMemory(device, mem, offset, size, flags, ppData);
    vktrace_set_packet_entrypoint_end_time(pHeader);
    pPacket = interpret_body_as_vkMapMemory(pHeader);
    pPacket->device = device;
    pPacket->mem = mem;
    pPacket->offset = offset;
    pPacket->size = size;
    pPacket->flags = flags;
    if (ppData != NULL)
    {
        vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->ppData), sizeof(void*), *ppData);
        vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->ppData));
        add_data_to_mem_info(mem, size, offset, *ppData);
    }
    pPacket->result = result;
    FINISH_TRACE_PACKET();
    return result;
}

VKTRACER_EXPORT void VKAPI __HOOKED_vkUnmapMemory(
    VkDevice device,
    VkDeviceMemory mem)
{
    vktrace_trace_packet_header* pHeader;
    packet_vkUnmapMemory* pPacket;
    VKAllocInfo *entry;
    size_t siz = 0, off = 0;
    // insert into packet the data that was written by CPU between the vkMapMemory call and here
    // Note must do this prior to the real vkUnMap() or else may get a FAULT
    vktrace_enter_critical_section(&g_memInfoLock);
    entry = find_mem_info_entry(mem);
    if (entry && entry->pData != NULL)
    {
        if (!entry->didFlush)
        {
            // no FlushMapped Memory
            siz = entry->rangeSize;
            off = entry->rangeOffset;
        }
    }
    CREATE_TRACE_PACKET(vkUnmapMemory, siz);
    pPacket = interpret_body_as_vkUnmapMemory(pHeader);
    if (siz)
    {
        assert(entry->handle.handle == mem.handle);
        vktrace_add_buffer_to_trace_packet(pHeader, (void**) &(pPacket->pData), siz, entry->pData + off);
        vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pData));
        entry->pData = NULL;
    }
    vktrace_leave_critical_section(&g_memInfoLock);
    mdd(device)->devTable.UnmapMemory(device, mem);
    vktrace_set_packet_entrypoint_end_time(pHeader);
    pPacket->device = device;
    pPacket->mem = mem;
    FINISH_TRACE_PACKET();
}

VKTRACER_EXPORT void VKAPI __HOOKED_vkFreeMemory(
    VkDevice device,
    VkDeviceMemory mem)
{
    vktrace_trace_packet_header* pHeader;
    packet_vkFreeMemory* pPacket = NULL;
    CREATE_TRACE_PACKET(vkFreeMemory, 0);
    mdd(device)->devTable.FreeMemory(device, mem);
    vktrace_set_packet_entrypoint_end_time(pHeader);
    pPacket = interpret_body_as_vkFreeMemory(pHeader);
    pPacket->device = device;
    pPacket->mem = mem;
    FINISH_TRACE_PACKET();
    // begin custom code
    rm_handle_from_mem_info(mem);
    // end custom code
}

VKTRACER_EXPORT VkResult VKAPI __HOOKED_vkFlushMappedMemoryRanges(
    VkDevice device,
    uint32_t memRangeCount,
    const VkMappedMemoryRange* pMemRanges)
{
    vktrace_trace_packet_header* pHeader;
    VkResult result;
    size_t rangesSize = 0;
    size_t dataSize = 0;
    uint32_t iter;
    packet_vkFlushMappedMemoryRanges* pPacket = NULL;

    // find out how much memory is in the ranges
    for (iter = 0; iter < memRangeCount; iter++)
    {
        VkMappedMemoryRange* pRange = (VkMappedMemoryRange*)&pMemRanges[iter];
        rangesSize += vk_size_vkmappedmemoryrange(pRange);
        dataSize += pRange->size;
    }

    CREATE_TRACE_PACKET(vkFlushMappedMemoryRanges, rangesSize + sizeof(void*)*memRangeCount + dataSize);
    pPacket = interpret_body_as_vkFlushMappedMemoryRanges(pHeader);

    vktrace_add_buffer_to_trace_packet(pHeader, (void**) &(pPacket->pMemRanges), rangesSize, pMemRanges);
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pMemRanges));

    // insert into packet the data that was written by CPU between the vkMapMemory call and here
    // create a temporary local ppData array and add it to the packet (to reserve the space for the array)
    void** ppTmpData = (void **) malloc(memRangeCount * sizeof(void*));
    vktrace_add_buffer_to_trace_packet(pHeader, (void**) &(pPacket->ppData), sizeof(void*)*memRangeCount, ppTmpData);
    free(ppTmpData);

    // now the actual memory
    vktrace_enter_critical_section(&g_memInfoLock);
    for (iter = 0; iter < memRangeCount; iter++)
    {
        VkMappedMemoryRange* pRange = (VkMappedMemoryRange*)&pMemRanges[iter];
        VKAllocInfo* pEntry = find_mem_info_entry(pRange->mem);

        if (pEntry != NULL)
        {
            assert(pEntry->handle.handle == pRange->mem.handle);
            assert(pEntry->totalSize >= (pRange->size + pRange->offset));
            assert(pEntry->totalSize >= pRange->size);
            assert(pRange->offset >= pEntry->rangeOffset && (pRange->offset + pRange->size) <= (pEntry->rangeOffset + pEntry->rangeSize));
            vktrace_add_buffer_to_trace_packet(pHeader, (void**) &(pPacket->ppData[iter]), pRange->size, pEntry->pData + pRange->offset);
            vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->ppData[iter]));
            pEntry->didFlush = TRUE;
        }
        else
        {
             vktrace_LogError("Failed to copy app memory into trace packet (idx = %u) on vkFlushedMappedMemoryRanges", pHeader->global_packet_index);
        }
    }
    vktrace_leave_critical_section(&g_memInfoLock);

    // now finalize the ppData array since it is done being updated
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->ppData));

    result = mdd(device)->devTable.FlushMappedMemoryRanges(device, memRangeCount, pMemRanges);
    vktrace_set_packet_entrypoint_end_time(pHeader);
    pPacket->device = device;
    pPacket->memRangeCount = memRangeCount;
    pPacket->result = result;
    FINISH_TRACE_PACKET();
    return result;
}

VKTRACER_EXPORT VkResult VKAPI __HOOKED_vkCreateDescriptorPool(
    VkDevice device,
    const VkDescriptorPoolCreateInfo* pCreateInfo,
    VkDescriptorPool* pDescriptorPool)
{
    vktrace_trace_packet_header* pHeader;
    VkResult result;
    packet_vkCreateDescriptorPool* pPacket = NULL;
    // begin custom code (needs to use get_struct_chain_size)
    CREATE_TRACE_PACKET(vkCreateDescriptorPool,  get_struct_chain_size((void*)pCreateInfo) + sizeof(VkDescriptorPool));
    // end custom code
    result = mdd(device)->devTable.CreateDescriptorPool(device, pCreateInfo, pDescriptorPool);
    vktrace_set_packet_entrypoint_end_time(pHeader);
    pPacket = interpret_body_as_vkCreateDescriptorPool(pHeader);
    pPacket->device = device;
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pCreateInfo), sizeof(VkDescriptorPoolCreateInfo), pCreateInfo);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pCreateInfo->pTypeCount), pCreateInfo->count * sizeof(VkDescriptorTypeCount), pCreateInfo->pTypeCount);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pDescriptorPool), sizeof(VkDescriptorPool), pDescriptorPool);
    pPacket->result = result;
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pCreateInfo->pTypeCount));
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pCreateInfo));
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pDescriptorPool));
    FINISH_TRACE_PACKET();
    return result;
}

VKTRACER_EXPORT VkResult VKAPI __HOOKED_vkCreateDevice(
    VkPhysicalDevice physicalDevice,
    const VkDeviceCreateInfo* pCreateInfo,
    VkDevice* pDevice)
{
    vktrace_trace_packet_header* pHeader;
    VkResult result;
    packet_vkCreateDevice* pPacket = NULL;

    CREATE_TRACE_PACKET(vkCreateDevice, get_struct_chain_size((void*)pCreateInfo) + sizeof(VkDevice));
    result = mdd(*pDevice)->devTable.CreateDevice(physicalDevice, pCreateInfo, pDevice);
    if (result == VK_SUCCESS)
        ext_init_create_device(mdd(*pDevice), *pDevice, pCreateInfo->extensionCount, pCreateInfo->ppEnabledExtensionNames);

    vktrace_set_packet_entrypoint_end_time(pHeader);
    pPacket = interpret_body_as_vkCreateDevice(pHeader);
    pPacket->physicalDevice = physicalDevice;
    add_VkDeviceCreateInfo_to_packet(pHeader, (VkDeviceCreateInfo**) &(pPacket->pCreateInfo), pCreateInfo);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pDevice), sizeof(VkDevice), pDevice);
    pPacket->result = result;
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pDevice));
    FINISH_TRACE_PACKET();
    return result;
}

VKTRACER_EXPORT VkResult VKAPI __HOOKED_vkCreateFramebuffer(
    VkDevice device,
    const VkFramebufferCreateInfo* pCreateInfo,
    VkFramebuffer* pFramebuffer)
{
    vktrace_trace_packet_header* pHeader;
    VkResult result;
    packet_vkCreateFramebuffer* pPacket = NULL;
    // begin custom code
    uint32_t attachmentCount = (pCreateInfo != NULL && pCreateInfo->pAttachments != NULL) ? pCreateInfo->attachmentCount : 0;
    CREATE_TRACE_PACKET(vkCreateFramebuffer, get_struct_chain_size((void*)pCreateInfo) + sizeof(VkFramebuffer));
    // end custom code
    result = mdd(device)->devTable.CreateFramebuffer(device, pCreateInfo, pFramebuffer);
    vktrace_set_packet_entrypoint_end_time(pHeader);
    pPacket = interpret_body_as_vkCreateFramebuffer(pHeader);
    pPacket->device = device;
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pCreateInfo), sizeof(VkFramebufferCreateInfo), pCreateInfo);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pCreateInfo->pAttachments), attachmentCount * sizeof(VkImageView), pCreateInfo->pAttachments);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pFramebuffer), sizeof(VkFramebuffer), pFramebuffer);
    pPacket->result = result;
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pCreateInfo->pAttachments));
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pCreateInfo));
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pFramebuffer));
    FINISH_TRACE_PACKET();
    return result;
}

VKTRACER_EXPORT VkResult VKAPI __HOOKED_vkCreateInstance(
    const VkInstanceCreateInfo* pCreateInfo,
    VkInstance* pInstance)
{
    vktrace_trace_packet_header* pHeader;
    VkResult result;
    packet_vkCreateInstance* pPacket = NULL;
    uint64_t startTime;
    uint64_t endTime;
    uint64_t vktraceStartTime = vktrace_get_time();
    SEND_ENTRYPOINT_ID(vkCreateInstance);
    startTime = vktrace_get_time();
    result = mid(*pInstance)->instTable.CreateInstance(pCreateInfo, pInstance);
    endTime = vktrace_get_time();
    if (result == VK_SUCCESS)
        ext_init_create_instance(mid(*pInstance), *pInstance, pCreateInfo->extensionCount, pCreateInfo->ppEnabledExtensionNames);

    CREATE_TRACE_PACKET(vkCreateInstance, sizeof(VkInstance) + get_struct_chain_size((void*)pCreateInfo));
    pHeader->vktrace_begin_time = vktraceStartTime;
    pHeader->entrypoint_begin_time = startTime;
    pHeader->entrypoint_end_time = endTime;
    pPacket = interpret_body_as_vkCreateInstance(pHeader);

    add_VkInstanceCreateInfo_to_packet(pHeader, (VkInstanceCreateInfo**)&(pPacket->pCreateInfo), (VkInstanceCreateInfo*) pCreateInfo);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pInstance), sizeof(VkInstance), pInstance);
    pPacket->result = result;
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pInstance));
    FINISH_TRACE_PACKET();
    return result;
}

VKTRACER_EXPORT VkResult VKAPI __HOOKED_vkCreateRenderPass(
    VkDevice device,
    const VkRenderPassCreateInfo* pCreateInfo,
    VkRenderPass* pRenderPass)
{
    vktrace_trace_packet_header* pHeader;
    VkResult result;
    packet_vkCreateRenderPass* pPacket = NULL;
    // begin custom code (get_struct_chain_size)
    uint32_t attachmentCount = (pCreateInfo != NULL && (pCreateInfo->pAttachments != NULL)) ? pCreateInfo->attachmentCount : 0;
    uint32_t dependencyCount = (pCreateInfo != NULL && (pCreateInfo->pDependencies != NULL)) ? pCreateInfo->dependencyCount : 0;
    uint32_t subpassCount = (pCreateInfo != NULL && (pCreateInfo->pSubpasses != NULL)) ? pCreateInfo->subpassCount : 0;
    CREATE_TRACE_PACKET(vkCreateRenderPass, get_struct_chain_size((void*)pCreateInfo) + sizeof(VkRenderPass));
    // end custom code
    result = mdd(device)->devTable.CreateRenderPass(device, pCreateInfo, pRenderPass);
    vktrace_set_packet_entrypoint_end_time(pHeader);
    pPacket = interpret_body_as_vkCreateRenderPass(pHeader);
    pPacket->device = device;
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pCreateInfo), sizeof(VkRenderPassCreateInfo), pCreateInfo);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pCreateInfo->pAttachments), attachmentCount * sizeof(VkAttachmentDescription), pCreateInfo->pAttachments);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pCreateInfo->pDependencies), dependencyCount * sizeof(VkSubpassDependency), pCreateInfo->pDependencies);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pCreateInfo->pSubpasses), subpassCount * sizeof(VkSubpassDescription), pCreateInfo->pSubpasses);
    uint32_t i;
    for (i=0; i < pPacket->pCreateInfo->subpassCount; i++) {
        VkSubpassDescription *pSubpass = (VkSubpassDescription *) &pPacket->pCreateInfo->pSubpasses[i];
        const VkSubpassDescription *pSp = &pCreateInfo->pSubpasses[i];
        vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pSubpass->pInputAttachments), pSubpass->inputCount * sizeof(VkAttachmentReference), pSp->pInputAttachments);
        vktrace_finalize_buffer_address(pHeader, (void**)&(pSubpass->pInputAttachments));
        vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pSubpass->pColorAttachments), pSubpass->colorCount * sizeof(VkAttachmentReference), pSp->pColorAttachments);
        vktrace_finalize_buffer_address(pHeader, (void**)&(pSubpass->pColorAttachments));
        vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pSubpass->pResolveAttachments), pSubpass->colorCount * sizeof(VkAttachmentReference), pSp->pResolveAttachments);
        vktrace_finalize_buffer_address(pHeader, (void**)&(pSubpass->pResolveAttachments));
        vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pSubpass->pPreserveAttachments), pSubpass->preserveCount * sizeof(VkAttachmentReference), pSp->pPreserveAttachments);
        vktrace_finalize_buffer_address(pHeader, (void**)&(pSubpass->pPreserveAttachments));
    }
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pRenderPass), sizeof(VkRenderPass), pRenderPass);
    pPacket->result = result;
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pCreateInfo->pAttachments));
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pCreateInfo->pDependencies));
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pCreateInfo->pSubpasses));
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pCreateInfo));
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pRenderPass));
    FINISH_TRACE_PACKET();
    return result;
}

VKTRACER_EXPORT VkResult VKAPI __HOOKED_vkEnumerateDeviceExtensionProperties(
    VkPhysicalDevice physicalDevice,
    const char* pLayerName,
    uint32_t* pCount,
    VkExtensionProperties* pProperties)
{
    vktrace_trace_packet_header* pHeader;
    VkResult result;
    packet_vkEnumerateDeviceExtensionProperties* pPacket = NULL;
    uint64_t startTime;
    uint64_t endTime;
    uint64_t vktraceStartTime = vktrace_get_time();
    startTime = vktrace_get_time();
    result = mid(physicalDevice)->instTable.EnumerateDeviceExtensionProperties(physicalDevice, pLayerName, pCount, pProperties);
    endTime = vktrace_get_time();
    CREATE_TRACE_PACKET(vkEnumerateDeviceExtensionProperties, ((pLayerName != NULL) ? strlen(pLayerName) + 1 : 0) + sizeof(uint32_t) + (*pCount * sizeof(VkExtensionProperties)));
    pHeader->vktrace_begin_time = vktraceStartTime;
    pHeader->entrypoint_begin_time = startTime;
    pHeader->entrypoint_end_time = endTime;
    pPacket = interpret_body_as_vkEnumerateDeviceExtensionProperties(pHeader);
    pPacket->physicalDevice = physicalDevice;
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pLayerName), ((pLayerName != NULL) ? strlen(pLayerName) + 1 : 0), pLayerName);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pCount), sizeof(uint32_t), pCount);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pProperties), *pCount * sizeof(VkExtensionProperties), pProperties);
    pPacket->result = result;
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pLayerName));
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pCount));
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pProperties));
    FINISH_TRACE_PACKET();
    return result;
}

VKTRACER_EXPORT VkResult VKAPI __HOOKED_vkEnumerateDeviceLayerProperties(
    VkPhysicalDevice physicalDevice,
    uint32_t* pCount,
    VkLayerProperties* pProperties)
{
    vktrace_trace_packet_header* pHeader;
    VkResult result;
    packet_vkEnumerateDeviceLayerProperties* pPacket = NULL;
    uint64_t startTime;
    uint64_t endTime;
    uint64_t vktraceStartTime = vktrace_get_time();
    startTime = vktrace_get_time();
    result = mid(physicalDevice)->instTable.EnumerateDeviceLayerProperties(physicalDevice, pCount, pProperties);
    endTime = vktrace_get_time();
    CREATE_TRACE_PACKET(vkEnumerateDeviceLayerProperties, sizeof(uint32_t) + (*pCount * sizeof(VkLayerProperties)));
    pHeader->vktrace_begin_time = vktraceStartTime;
    pHeader->entrypoint_begin_time = startTime;
    pHeader->entrypoint_end_time = endTime;
    pPacket = interpret_body_as_vkEnumerateDeviceLayerProperties(pHeader);
    pPacket->physicalDevice = physicalDevice;
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pCount), sizeof(uint32_t), pCount);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pProperties), *pCount * sizeof(VkLayerProperties), pProperties);
    pPacket->result = result;
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pCount));
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pProperties));
    FINISH_TRACE_PACKET();
    return result;
}
// TODO : This should be pretty easy to fit into codegen. Don't need to make the call prior to creating packet
//  Just need to account for "count" number of queue properties
VKTRACER_EXPORT VkResult VKAPI __HOOKED_vkGetPhysicalDeviceQueueFamilyProperties(
    VkPhysicalDevice physicalDevice,
    uint32_t* pCount,
    VkQueueFamilyProperties* pQueueFamilyProperties)
{
    vktrace_trace_packet_header* pHeader;
    VkResult result;
    packet_vkGetPhysicalDeviceQueueFamilyProperties* pPacket = NULL;
    uint64_t startTime;
    uint64_t endTime;
    uint64_t vktraceStartTime = vktrace_get_time();
    startTime = vktrace_get_time();
    result = mid(physicalDevice)->instTable.GetPhysicalDeviceQueueFamilyProperties(physicalDevice, pCount, pQueueFamilyProperties);
    endTime = vktrace_get_time();
    CREATE_TRACE_PACKET(vkGetPhysicalDeviceQueueFamilyProperties, sizeof(uint32_t) + *pCount * sizeof(VkQueueFamilyProperties));
    pHeader->vktrace_begin_time = vktraceStartTime;
    pHeader->entrypoint_begin_time = startTime;
    pHeader->entrypoint_end_time = endTime;
    pPacket = interpret_body_as_vkGetPhysicalDeviceQueueFamilyProperties(pHeader);
    pPacket->physicalDevice = physicalDevice;
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pCount), sizeof(uint32_t), pCount);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pQueueFamilyProperties), *pCount * sizeof(VkQueueFamilyProperties), pQueueFamilyProperties);
    pPacket->result = result;
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pCount));
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pQueueFamilyProperties));
    FINISH_TRACE_PACKET();
    return result;
}

VKTRACER_EXPORT VkResult VKAPI __HOOKED_vkEnumeratePhysicalDevices(
    VkInstance instance,
    uint32_t* pPhysicalDeviceCount,
    VkPhysicalDevice* pPhysicalDevices)
{
    vktrace_trace_packet_header* pHeader;
    VkResult result;
    packet_vkEnumeratePhysicalDevices* pPacket = NULL;
    uint64_t startTime;
    uint64_t endTime;
    uint64_t vktraceStartTime = vktrace_get_time();
    //TODO make sure can handle being called twice with pPD == 0
    SEND_ENTRYPOINT_ID(vkEnumeratePhysicalDevices);
    startTime = vktrace_get_time();
    result = mid(instance)->instTable.EnumeratePhysicalDevices(instance, pPhysicalDeviceCount, pPhysicalDevices);
    endTime = vktrace_get_time();
    CREATE_TRACE_PACKET(vkEnumeratePhysicalDevices, sizeof(uint32_t) + ((pPhysicalDevices && pPhysicalDeviceCount) ? *pPhysicalDeviceCount * sizeof(VkPhysicalDevice) : 0));
    pHeader->vktrace_begin_time = vktraceStartTime;
    pHeader->entrypoint_begin_time = startTime;
    pHeader->entrypoint_end_time = endTime;
    pPacket = interpret_body_as_vkEnumeratePhysicalDevices(pHeader);
    pPacket->instance = instance;
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pPhysicalDeviceCount), sizeof(uint32_t), pPhysicalDeviceCount);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pPhysicalDevices), *pPhysicalDeviceCount*sizeof(VkPhysicalDevice), pPhysicalDevices);
    pPacket->result = result;
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pPhysicalDeviceCount));
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pPhysicalDevices));
    FINISH_TRACE_PACKET();
    return result;
}

VKTRACER_EXPORT VkResult VKAPI __HOOKED_vkGetQueryPoolResults(
    VkDevice device,
    VkQueryPool queryPool,
    uint32_t startQuery,
    uint32_t queryCount,
    size_t* pDataSize,
    void* pData,
    VkQueryResultFlags flags)
{
    vktrace_trace_packet_header* pHeader;
    VkResult result;
    size_t _dataSize;
    packet_vkGetQueryPoolResults* pPacket = NULL;
    uint64_t startTime;
    uint64_t endTime;
    uint64_t vktraceStartTime = vktrace_get_time();
    startTime = vktrace_get_time();
    result = mdd(device)->devTable.GetQueryPoolResults(device, queryPool, startQuery, queryCount, pDataSize, pData, flags);
    endTime = vktrace_get_time();
    _dataSize = (pDataSize == NULL || pData == NULL) ? 0 : *pDataSize;
    CREATE_TRACE_PACKET(vkGetQueryPoolResults, ((pDataSize != NULL) ? sizeof(size_t) : 0) + _dataSize);
    pHeader->vktrace_begin_time = vktraceStartTime;
    pHeader->entrypoint_begin_time = startTime;
    pHeader->entrypoint_end_time = endTime;
    pPacket = interpret_body_as_vkGetQueryPoolResults(pHeader);
    pPacket->device = device;
    pPacket->queryPool = queryPool;
    pPacket->startQuery = startQuery;
    pPacket->queryCount = queryCount;
    pPacket->flags = flags;
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pDataSize), sizeof(size_t), &_dataSize);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pData), _dataSize, pData);
    pPacket->result = result;
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pDataSize));
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pData));
    FINISH_TRACE_PACKET();
    return result;
}

VKTRACER_EXPORT VkResult VKAPI __HOOKED_vkAllocDescriptorSets(
    VkDevice device,
    VkDescriptorPool descriptorPool,
    VkDescriptorSetUsage setUsage,
    uint32_t count,
    const VkDescriptorSetLayout* pSetLayouts,
    VkDescriptorSet* pDescriptorSets)
{
    vktrace_trace_packet_header* pHeader;
    VkResult result;
    packet_vkAllocDescriptorSets* pPacket = NULL;
    uint64_t startTime;
    uint64_t endTime;
    uint64_t vktraceStartTime = vktrace_get_time();
    SEND_ENTRYPOINT_ID(vkAllocDescriptorSets);
    startTime = vktrace_get_time();
    result = mdd(device)->devTable.AllocDescriptorSets(device, descriptorPool, setUsage, count, pSetLayouts, pDescriptorSets);
    endTime = vktrace_get_time();
    CREATE_TRACE_PACKET(vkAllocDescriptorSets, (count * sizeof(VkDescriptorSetLayout)) + (count * sizeof(VkDescriptorSet)));
    pHeader->vktrace_begin_time = vktraceStartTime;
    pHeader->entrypoint_begin_time = startTime;
    pHeader->entrypoint_end_time = endTime;
    pPacket = interpret_body_as_vkAllocDescriptorSets(pHeader);
    pPacket->device = device;
    pPacket->descriptorPool = descriptorPool;
    pPacket->setUsage = setUsage;
    pPacket->count = count;
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pSetLayouts), count * sizeof(VkDescriptorSetLayout), pSetLayouts);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pDescriptorSets), count * sizeof(VkDescriptorSet), pDescriptorSets);
    pPacket->result = result;
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pSetLayouts));
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pDescriptorSets));
    FINISH_TRACE_PACKET();
    return result;
}

VKTRACER_EXPORT void VKAPI __HOOKED_vkUpdateDescriptorSets(
VkDevice device,
        uint32_t writeCount,
        const VkWriteDescriptorSet* pDescriptorWrites,
        uint32_t copyCount,
        const VkCopyDescriptorSet* pDescriptorCopies);
// Manually written because it needs to use get_struct_chain_size and allocate some extra pointers (why?)
// Also since it needs to app the array of pointers and sub-buffers (see comments in function)
VKTRACER_EXPORT void VKAPI __HOOKED_vkUpdateDescriptorSets(
    VkDevice device,
    uint32_t writeCount,
    const VkWriteDescriptorSet* pDescriptorWrites,
    uint32_t copyCount,
    const VkCopyDescriptorSet* pDescriptorCopies )
{
    vktrace_trace_packet_header* pHeader;
    packet_vkUpdateDescriptorSets* pPacket = NULL;
    // begin custom code
    size_t arrayByteCount = 0;
    size_t i;

    for (i = 0; i < writeCount; i++)
    {
        arrayByteCount += get_struct_chain_size(&pDescriptorWrites[i]);
    }

    for (i = 0; i < copyCount; i++)
    {
        arrayByteCount += get_struct_chain_size(&pDescriptorCopies[i]);
    }

    CREATE_TRACE_PACKET(vkUpdateDescriptorSets, arrayByteCount);
    // end custom code

    mdd(device)->devTable.UpdateDescriptorSets(device, writeCount, pDescriptorWrites, copyCount, pDescriptorCopies);
    vktrace_set_packet_entrypoint_end_time(pHeader);
    pPacket = interpret_body_as_vkUpdateDescriptorSets(pHeader);
    pPacket->device = device;
    pPacket->writeCount = writeCount;
    // begin custom code
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pDescriptorWrites), writeCount * sizeof(VkWriteDescriptorSet), pDescriptorWrites);
    for (i = 0; i < writeCount; i++)
    {
        vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pDescriptorWrites[i].pDescriptors), pDescriptorWrites[i].count * sizeof(VkDescriptorInfo), pDescriptorWrites[i].pDescriptors);
        vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pDescriptorWrites[i].pDescriptors));
    }
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pDescriptorWrites));

    pPacket->copyCount = copyCount;
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pDescriptorCopies), copyCount * sizeof(VkCopyDescriptorSet), pDescriptorCopies);
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pDescriptorCopies));
    // end custom code
    FINISH_TRACE_PACKET();
}

VKTRACER_EXPORT void VKAPI __HOOKED_vkCmdWaitEvents(
    VkCmdBuffer                                 cmdBuffer,
    uint32_t                                    eventCount,
    const VkEvent*                              pEvents,
    VkPipelineStageFlags                        srcStageMask,
    VkPipelineStageFlags                        destStageMask,
    uint32_t                                    memBarrierCount,
    const void* const*                          ppMemBarriers)
{
    vktrace_trace_packet_header* pHeader;
    packet_vkCmdWaitEvents* pPacket = NULL;
    size_t customSize;
    customSize = (eventCount * sizeof(VkEvent)) + memBarrierCount * sizeof(void*) + calculate_memory_barrier_size(memBarrierCount, ppMemBarriers);
    CREATE_TRACE_PACKET(vkCmdWaitEvents, customSize);
    mdd(cmdBuffer)->devTable.CmdWaitEvents(cmdBuffer, eventCount, pEvents, srcStageMask, destStageMask, memBarrierCount, ppMemBarriers);
    vktrace_set_packet_entrypoint_end_time(pHeader);
    pPacket = interpret_body_as_vkCmdWaitEvents(pHeader);
    pPacket->cmdBuffer = cmdBuffer;
    pPacket->eventCount = eventCount;
    pPacket->srcStageMask = srcStageMask;
    pPacket->destStageMask = destStageMask;
    pPacket->memBarrierCount = memBarrierCount;
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pEvents), eventCount * sizeof(VkEvent), pEvents);
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pEvents));
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->ppMemBarriers), memBarrierCount * sizeof(void*), ppMemBarriers);
    uint32_t i, siz;
    for (i = 0; i < memBarrierCount; i++) {
        VkMemoryBarrier *pNext = (VkMemoryBarrier *) ppMemBarriers[i];
        switch (pNext->sType) {
            case VK_STRUCTURE_TYPE_MEMORY_BARRIER:
                siz = sizeof(VkMemoryBarrier);
                break;
            case VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER:
                siz = sizeof(VkBufferMemoryBarrier);
                break;
            case VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER:
                siz = sizeof(VkImageMemoryBarrier);
                break;
            default:
                assert(0);
                siz = 0;
                break;
        }
        vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->ppMemBarriers[i]), siz, ppMemBarriers[i]);
        vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->ppMemBarriers[i]));
    }
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->ppMemBarriers));
    FINISH_TRACE_PACKET();
}

VKTRACER_EXPORT void VKAPI __HOOKED_vkCmdPipelineBarrier(
    VkCmdBuffer                                 cmdBuffer,
    VkPipelineStageFlags                        srcStageMask,
    VkPipelineStageFlags                        destStageMask,
    VkBool32                                    byRegion,
    uint32_t                                    memBarrierCount,
    const void* const*                          ppMemBarriers)
{
    vktrace_trace_packet_header* pHeader;
    packet_vkCmdPipelineBarrier* pPacket = NULL;
    size_t customSize;
    customSize = (memBarrierCount * sizeof(void*)) + calculate_memory_barrier_size(memBarrierCount, ppMemBarriers);
    CREATE_TRACE_PACKET(vkCmdPipelineBarrier, customSize);
    mdd(cmdBuffer)->devTable.CmdPipelineBarrier(cmdBuffer, srcStageMask, destStageMask, byRegion, memBarrierCount, ppMemBarriers);
    vktrace_set_packet_entrypoint_end_time(pHeader);
    pPacket = interpret_body_as_vkCmdPipelineBarrier(pHeader);
    pPacket->cmdBuffer = cmdBuffer;
    pPacket->srcStageMask = srcStageMask;
    pPacket->destStageMask = destStageMask;
    pPacket->byRegion = byRegion;
    pPacket->memBarrierCount = memBarrierCount;
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->ppMemBarriers), memBarrierCount * sizeof(void*), ppMemBarriers);
    uint32_t i, siz;
    for (i = 0; i < memBarrierCount; i++) {
        VkMemoryBarrier *pNext = (VkMemoryBarrier *) ppMemBarriers[i];
        switch (pNext->sType) {
            case VK_STRUCTURE_TYPE_MEMORY_BARRIER:
                siz = sizeof(VkMemoryBarrier);
                break;
            case VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER:
                siz = sizeof(VkBufferMemoryBarrier);
                break;
            case VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER:
                siz = sizeof(VkImageMemoryBarrier);
                break;
            default:
                assert(0);
                siz = 0;
                break;
        }
        vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->ppMemBarriers[i]), siz, ppMemBarriers[i]);
        vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->ppMemBarriers[i]));
    }
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->ppMemBarriers));
    FINISH_TRACE_PACKET();
}

VKTRACER_EXPORT VkResult VKAPI __HOOKED_vkCreateGraphicsPipelines(
    VkDevice device,
    VkPipelineCache pipelineCache,
    uint32_t count,
    const VkGraphicsPipelineCreateInfo* pCreateInfos,
    VkPipeline* pPipelines)
{
    vktrace_trace_packet_header* pHeader;
    VkResult result;
    packet_vkCreateGraphicsPipelines* pPacket = NULL;
    size_t total_size = 0;
    uint32_t i;
    for (i = 0; i < count; i++) {
        total_size += get_struct_chain_size((void*)&pCreateInfos[i]);
    }
    CREATE_TRACE_PACKET(vkCreateGraphicsPipelines, total_size + count*sizeof(VkPipeline));
    result = mdd(device)->devTable.CreateGraphicsPipelines(device, pipelineCache, count, pCreateInfos, pPipelines);
    vktrace_set_packet_entrypoint_end_time(pHeader);
    pPacket = interpret_body_as_vkCreateGraphicsPipelines(pHeader);
    pPacket->device = device;
    pPacket->pipelineCache = pipelineCache;
    pPacket->count = count;
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pCreateInfos), count*sizeof(VkGraphicsPipelineCreateInfo), pCreateInfos);
    add_VkGraphicsPipelineCreateInfos_to_trace_packet(pHeader, (VkGraphicsPipelineCreateInfo*)pPacket->pCreateInfos, pCreateInfos, count);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pPipelines), count*sizeof(VkPipeline), pPipelines);
    pPacket->result = result;
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pCreateInfos));
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pPipelines));
    FINISH_TRACE_PACKET();
    return result;
}

VKTRACER_EXPORT VkResult VKAPI __HOOKED_vkCreateComputePipelines(
    VkDevice device,
    VkPipelineCache pipelineCache,
    uint32_t count,
    const VkComputePipelineCreateInfo* pCreateInfos,
    VkPipeline* pPipelines)
{
    vktrace_trace_packet_header* pHeader;
    VkResult result;
    packet_vkCreateComputePipelines* pPacket = NULL;
    CREATE_TRACE_PACKET(vkCreateComputePipelines, count*sizeof(VkComputePipelineCreateInfo) + sizeof(VkPipeline));
    result = mdd(device)->devTable.CreateComputePipelines(device, pipelineCache, count, pCreateInfos, pPipelines);
    vktrace_set_packet_entrypoint_end_time(pHeader);
    pPacket = interpret_body_as_vkCreateComputePipelines(pHeader);
    pPacket->device = device;
    pPacket->pipelineCache = pipelineCache;
    pPacket->count = count;
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pCreateInfos), count*sizeof(VkComputePipelineCreateInfo), pCreateInfos);
    add_VkComputePipelineCreateInfos_to_trace_packet(pHeader, (VkComputePipelineCreateInfo*)pPacket->pCreateInfos, pCreateInfos, count);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pPipelines), count*sizeof(VkPipeline), pPipelines);
    pPacket->result = result;
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pCreateInfos));
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pPipelines));
    FINISH_TRACE_PACKET();
    return result;
}

VKTRACER_EXPORT void VKAPI __HOOKED_vkCmdBeginRenderPass(
    VkCmdBuffer cmdBuffer,
    const VkRenderPassBeginInfo* pRenderPassBegin,
    VkRenderPassContents contents)
{
    vktrace_trace_packet_header* pHeader;
    packet_vkCmdBeginRenderPass* pPacket = NULL;
    size_t clearValueSize = sizeof(VkClearValue) * pRenderPassBegin->clearValueCount;
    CREATE_TRACE_PACKET(vkCmdBeginRenderPass, sizeof(VkRenderPassBeginInfo) + clearValueSize);
    mdd(cmdBuffer)->devTable.CmdBeginRenderPass(cmdBuffer, pRenderPassBegin, contents);
    vktrace_set_packet_entrypoint_end_time(pHeader);
    pPacket = interpret_body_as_vkCmdBeginRenderPass(pHeader);
    pPacket->cmdBuffer = cmdBuffer;
    pPacket->contents = contents;
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pRenderPassBegin), sizeof(VkRenderPassBeginInfo), pRenderPassBegin);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pRenderPassBegin->pClearValues), clearValueSize, pRenderPassBegin->pClearValues);
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pRenderPassBegin->pClearValues));
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pRenderPassBegin));
    FINISH_TRACE_PACKET();
}

VKTRACER_EXPORT VkResult VKAPI __HOOKED_vkFreeDescriptorSets(
    VkDevice device,
    VkDescriptorPool descriptorPool,
    uint32_t count,
    const VkDescriptorSet* pDescriptorSets)
{
    vktrace_trace_packet_header* pHeader;
    VkResult result;
    packet_vkFreeDescriptorSets* pPacket = NULL;
    CREATE_TRACE_PACKET(vkFreeDescriptorSets, count*sizeof(VkDescriptorSet));
    result = mdd(device)->devTable.FreeDescriptorSets(device, descriptorPool, count, pDescriptorSets);
    vktrace_set_packet_entrypoint_end_time(pHeader);
    pPacket = interpret_body_as_vkFreeDescriptorSets(pHeader);
    pPacket->device = device;
    pPacket->descriptorPool = descriptorPool;
    pPacket->count = count;
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pDescriptorSets), count*sizeof(VkDescriptorSet), pDescriptorSets);
    pPacket->result = result;
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pDescriptorSets));
    FINISH_TRACE_PACKET();
    return result;
}

VKTRACER_EXPORT VkResult VKAPI __HOOKED_vkGetSurfacePropertiesKHR(
    VkDevice device,
    const VkSurfaceDescriptionKHR* pSurfaceDescription,
    VkSurfacePropertiesKHR* pSurfaceProperties)
{
    vktrace_trace_packet_header* pHeader;
    VkResult result;
    packet_vkGetSurfacePropertiesKHR* pPacket = NULL;
    CREATE_TRACE_PACKET(vkGetSurfacePropertiesKHR, sizeof(VkSurfaceDescriptionKHR) + sizeof(VkSurfacePropertiesKHR));
    result = mdd(device)->devTable.GetSurfacePropertiesKHR(device, pSurfaceDescription, pSurfaceProperties);
    pPacket = interpret_body_as_vkGetSurfacePropertiesKHR(pHeader);
    pPacket->device = device;
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pSurfaceDescription), sizeof(VkSurfaceDescriptionKHR), pSurfaceDescription);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pSurfaceProperties), sizeof(VkSurfacePropertiesKHR), pSurfaceProperties);
    pPacket->result = result;
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pSurfaceDescription));
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pSurfaceProperties));
    FINISH_TRACE_PACKET();
    return result;
}

VKTRACER_EXPORT VkResult VKAPI __HOOKED_vkGetSurfaceFormatsKHR(
    VkDevice device,
    const VkSurfaceDescriptionKHR* pSurfaceDescription,
    uint32_t* pCount,
    VkSurfaceFormatKHR* pSurfaceFormats)
{
    vktrace_trace_packet_header* pHeader;
    VkResult result;
    size_t _dataSize;
    packet_vkGetSurfaceFormatsKHR* pPacket = NULL;
    uint64_t startTime;
    uint64_t endTime;
    uint64_t vktraceStartTime = vktrace_get_time();
    startTime = vktrace_get_time();
    result = mdd(device)->devTable.GetSurfaceFormatsKHR(device, pSurfaceDescription, pCount, pSurfaceFormats);
    endTime = vktrace_get_time();
    _dataSize = (pCount == NULL || pSurfaceFormats == NULL) ? 0 : (*pCount *sizeof(VkSurfaceFormatKHR));
    CREATE_TRACE_PACKET(vkGetSurfaceFormatsKHR, sizeof(VkSurfaceDescriptionKHR) + sizeof(uint32_t) + _dataSize);
    pHeader->vktrace_begin_time = vktraceStartTime;
    pHeader->entrypoint_begin_time = startTime;
    pHeader->entrypoint_end_time = endTime;
    pPacket = interpret_body_as_vkGetSurfaceFormatsKHR(pHeader);
    pPacket->device = device;
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pSurfaceDescription), sizeof(VkSurfaceDescriptionKHR), pSurfaceDescription);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pCount), sizeof(uint32_t), pCount);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pSurfaceFormats), _dataSize, pSurfaceFormats);
    pPacket->result = result;
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pSurfaceDescription));
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pCount));
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pSurfaceFormats));
    FINISH_TRACE_PACKET();
    return result;
}

VKTRACER_EXPORT VkResult VKAPI __HOOKED_vkGetSurfacePresentModesKHR(
    VkDevice device,
    const VkSurfaceDescriptionKHR* pSurfaceDescription,
    uint32_t* pCount,
    VkPresentModeKHR* pPresentModes)
{
    vktrace_trace_packet_header* pHeader;
    VkResult result;
    size_t _dataSize;
    packet_vkGetSurfacePresentModesKHR* pPacket = NULL;
    uint64_t startTime;
    uint64_t endTime;
    uint64_t vktraceStartTime = vktrace_get_time();
    startTime = vktrace_get_time();
    result = mdd(device)->devTable.GetSurfacePresentModesKHR(device, pSurfaceDescription, pCount, pPresentModes);
    endTime = vktrace_get_time();
    _dataSize = (pCount == NULL || pPresentModes == NULL) ? 0 : (*pCount *sizeof(VkPresentModeKHR));
    CREATE_TRACE_PACKET(vkGetSurfacePresentModesKHR, sizeof(VkSurfaceDescriptionKHR) + sizeof(uint32_t) + _dataSize);
    pHeader->vktrace_begin_time = vktraceStartTime;
    pHeader->entrypoint_begin_time = startTime;
    pHeader->entrypoint_end_time = endTime;
    pPacket = interpret_body_as_vkGetSurfacePresentModesKHR(pHeader);
    pPacket->device = device;
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pSurfaceDescription), sizeof(VkSurfaceDescriptionKHR), pSurfaceDescription);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pCount), sizeof(uint32_t), pCount);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pPresentModes), _dataSize, pPresentModes);
    pPacket->result = result;
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pSurfaceDescription));
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pCount));
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pPresentModes));
    FINISH_TRACE_PACKET();
    return result;
}

VKTRACER_EXPORT VkResult VKAPI __HOOKED_vkCreateSwapchainKHR(
    VkDevice device,
    const VkSwapchainCreateInfoKHR* pCreateInfo,
    VkSwapchainKHR* pSwapchain)
{
    vktrace_trace_packet_header* pHeader;
    VkResult result;
    packet_vkCreateSwapchainKHR* pPacket = NULL;
    CREATE_TRACE_PACKET(vkCreateSwapchainKHR, vk_ext_khr_device_swapchain_size_vkswapchaincreateinfokhr(pCreateInfo) + sizeof(VkSwapchainKHR));
    result = mdd(device)->devTable.CreateSwapchainKHR(device, pCreateInfo, pSwapchain);
    pPacket = interpret_body_as_vkCreateSwapchainKHR(pHeader);
    pPacket->device = device;
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pCreateInfo), sizeof(VkSwapchainCreateInfoKHR), pCreateInfo);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pCreateInfo->pSurfaceDescription), sizeof(VkSurfaceDescriptionKHR), pCreateInfo->pSurfaceDescription);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pSwapchain), sizeof(VkSwapchainKHR), pSwapchain);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pCreateInfo->pQueueFamilyIndices), pCreateInfo->queueFamilyCount * sizeof(uint32_t), pCreateInfo->pQueueFamilyIndices);
    pPacket->result = result;
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pCreateInfo->pSurfaceDescription));
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pCreateInfo->pQueueFamilyIndices));
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pCreateInfo));
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pSwapchain));
    FINISH_TRACE_PACKET();
    return result;
}

VKTRACER_EXPORT VkResult VKAPI __HOOKED_vkGetSwapchainImagesKHR(
    VkDevice device,
    VkSwapchainKHR swapchain,
    uint32_t* pCount,
    VkImage* pSwapchainImages)
{
    vktrace_trace_packet_header* pHeader;
    VkResult result;
    size_t _dataSize;
    packet_vkGetSwapchainImagesKHR* pPacket = NULL;
    uint64_t startTime;
    uint64_t endTime;
    uint64_t vktraceStartTime = vktrace_get_time();
    startTime = vktrace_get_time();
    result = mdd(device)->devTable.GetSwapchainImagesKHR(device, swapchain, pCount, pSwapchainImages);
    endTime = vktrace_get_time();
    _dataSize = (pCount == NULL || pSwapchainImages == NULL) ? 0 : (*pCount *sizeof(VkImage));
    CREATE_TRACE_PACKET(vkGetSwapchainImagesKHR, sizeof(uint32_t) + _dataSize);
    pHeader->vktrace_begin_time = vktraceStartTime;
    pHeader->entrypoint_begin_time = startTime;
    pHeader->entrypoint_end_time = endTime;
    pPacket = interpret_body_as_vkGetSwapchainImagesKHR(pHeader);
    pPacket->device = device;
    pPacket->swapchain = swapchain;
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pCount), sizeof(uint32_t), pCount);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pSwapchainImages), _dataSize, pSwapchainImages);
    pPacket->result = result;
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pCount));
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pSwapchainImages));
    FINISH_TRACE_PACKET();
    return result;
}

VKTRACER_EXPORT VkResult VKAPI __HOOKED_vkQueuePresentKHR(
    VkQueue queue,
    VkPresentInfoKHR* pPresentInfo)
{
    vktrace_trace_packet_header* pHeader;
    VkResult result;
    packet_vkQueuePresentKHR* pPacket = NULL;
    size_t swapchainSize = pPresentInfo->swapchainCount*sizeof(VkSwapchainKHR);
    size_t indexSize = pPresentInfo->swapchainCount*sizeof(uint32_t);
    CREATE_TRACE_PACKET(vkQueuePresentKHR, sizeof(VkPresentInfoKHR)+swapchainSize+indexSize);
    result = mdd(queue)->devTable.QueuePresentKHR(queue, pPresentInfo);
    vktrace_set_packet_entrypoint_end_time(pHeader);
    pPacket = interpret_body_as_vkQueuePresentKHR(pHeader);
    pPacket->queue = queue;
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pPresentInfo), sizeof(VkPresentInfoKHR), pPresentInfo);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pPresentInfo->swapchains), swapchainSize, pPresentInfo->swapchains);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pPresentInfo->imageIndices), indexSize, pPresentInfo->imageIndices);
    pPacket->result = result;
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pPresentInfo->imageIndices));
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pPresentInfo->swapchains));
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pPresentInfo));
    FINISH_TRACE_PACKET();
    return result;
}

/* TODO: Probably want to make this manual to get the result of the boolean and then check it on replay
VKTRACER_EXPORT VkResult VKAPI __HOOKED_vkGetPhysicalDeviceSurfaceSupportKHR(
    VkPhysicalDevice physicalDevice,
    uint32_t queueFamilyIndex,
    const VkSurfaceDescriptionKHR* pSurfaceDescription,
    VkBool32* pSupported)
{
    vktrace_trace_packet_header* pHeader;
    VkResult result;
    packet_vkGetPhysicalDeviceSurfaceSupportKHR* pPacket = NULL;
    CREATE_TRACE_PACKET(vkGetPhysicalDeviceSurfaceSupportKHR, sizeof(VkSurfaceDescriptionKHR) + sizeof(VkBool32));
    result = mid(physicalDevice)->instTable.GetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIndex, pSurfaceDescription, pSupported);
    vktrace_set_packet_entrypoint_end_time(pHeader);
    pPacket = interpret_body_as_vkGetPhysicalDeviceSurfaceSupportKHR(pHeader);
    pPacket->physicalDevice = physicalDevice;
    pPacket->queueFamilyIndex = queueFamilyIndex;
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pSurfaceDescription), sizeof(VkSurfaceDescriptionKHR), pSurfaceDescription);
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pSupported), sizeof(VkBool32), pSupported);
    pPacket->result = result;
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pSurfaceDescription));
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pSupported));
    FINISH_TRACE_PACKET();
    return result;
}
*/

static inline PFN_vkVoidFunction layer_intercept_proc(const char *name)
{
    if (!name || name[0] != 'v' || name[1] != 'k')
        return NULL;

    name += 2;

    if (!strcmp(name, "CreateDevice"))
        return (PFN_vkVoidFunction) __HOOKED_vkCreateDevice;
    if (!strcmp(name, "DestroyDevice"))
        return (PFN_vkVoidFunction) __HOOKED_vkDestroyDevice;
    if (!strcmp(name, "GetDeviceQueue"))
        return (PFN_vkVoidFunction) __HOOKED_vkGetDeviceQueue;
    if (!strcmp(name, "QueueSubmit"))
        return (PFN_vkVoidFunction) __HOOKED_vkQueueSubmit;
    if (!strcmp(name, "QueueWaitIdle"))
        return (PFN_vkVoidFunction) __HOOKED_vkQueueWaitIdle;
    if (!strcmp(name, "DeviceWaitIdle"))
        return (PFN_vkVoidFunction) __HOOKED_vkDeviceWaitIdle;
    if (!strcmp(name, "AllocMemory"))
        return (PFN_vkVoidFunction) __HOOKED_vkAllocMemory;
    if (!strcmp(name, "FreeMemory"))
        return (PFN_vkVoidFunction) __HOOKED_vkFreeMemory;
    if (!strcmp(name, "MapMemory"))
        return (PFN_vkVoidFunction) __HOOKED_vkMapMemory;
    if (!strcmp(name, "UnmapMemory"))
        return (PFN_vkVoidFunction) __HOOKED_vkUnmapMemory;
    if (!strcmp(name, "FlushMappedMemoryRanges"))
        return (PFN_vkVoidFunction) __HOOKED_vkFlushMappedMemoryRanges;
    if (!strcmp(name, "InvalidateMappedMemoryRanges"))
        return (PFN_vkVoidFunction) __HOOKED_vkInvalidateMappedMemoryRanges;
    if (!strcmp(name, "GetDeviceMemoryCommitment"))
        return (PFN_vkVoidFunction) __HOOKED_vkGetDeviceMemoryCommitment;
    if (!strcmp(name, "BindBufferMemory"))
        return (PFN_vkVoidFunction) __HOOKED_vkBindBufferMemory;
    if (!strcmp(name, "BindImageMemory"))
        return (PFN_vkVoidFunction) __HOOKED_vkBindImageMemory;
    if (!strcmp(name, "GetBufferMemoryRequirements"))
        return (PFN_vkVoidFunction) __HOOKED_vkGetBufferMemoryRequirements;
    if (!strcmp(name, "GetImageMemoryRequirements"))
        return (PFN_vkVoidFunction) __HOOKED_vkGetImageMemoryRequirements;
    if (!strcmp(name, "GetImageSparseMemoryRequirements"))
        return (PFN_vkVoidFunction) __HOOKED_vkGetImageSparseMemoryRequirements;
    if (!strcmp(name, "GetPhysicalDeviceSparseImageFormatProperties"))
        return (PFN_vkVoidFunction) __HOOKED_vkGetPhysicalDeviceSparseImageFormatProperties;
    if (!strcmp(name, "QueueBindSparseBufferMemory"))
        return (PFN_vkVoidFunction) __HOOKED_vkQueueBindSparseBufferMemory;
    if (!strcmp(name, "QueueBindSparseImageOpaqueMemory"))
        return (PFN_vkVoidFunction) __HOOKED_vkQueueBindSparseImageOpaqueMemory;
    if (!strcmp(name, "QueueBindSparseImageMemory"))
        return (PFN_vkVoidFunction) __HOOKED_vkQueueBindSparseImageMemory;
    if (!strcmp(name, "CreateFence"))
        return (PFN_vkVoidFunction) __HOOKED_vkCreateFence;
    if (!strcmp(name, "DestroyFence"))
        return (PFN_vkVoidFunction) __HOOKED_vkDestroyFence;
    if (!strcmp(name, "ResetFences"))
        return (PFN_vkVoidFunction) __HOOKED_vkResetFences;
    if (!strcmp(name, "GetFenceStatus"))
        return (PFN_vkVoidFunction) __HOOKED_vkGetFenceStatus;
    if (!strcmp(name, "WaitForFences"))
        return (PFN_vkVoidFunction) __HOOKED_vkWaitForFences;
    if (!strcmp(name, "CreateSemaphore"))
        return (PFN_vkVoidFunction) __HOOKED_vkCreateSemaphore;
    if (!strcmp(name, "DestroySemaphore"))
        return (PFN_vkVoidFunction) __HOOKED_vkDestroySemaphore;
    if (!strcmp(name, "QueueSignalSemaphore"))
        return (PFN_vkVoidFunction) __HOOKED_vkQueueSignalSemaphore;
    if (!strcmp(name, "QueueWaitSemaphore"))
        return (PFN_vkVoidFunction) __HOOKED_vkQueueWaitSemaphore;
    if (!strcmp(name, "CreateEvent"))
        return (PFN_vkVoidFunction) __HOOKED_vkCreateEvent;
    if (!strcmp(name, "DestroyEvent"))
        return (PFN_vkVoidFunction) __HOOKED_vkDestroyEvent;
    if (!strcmp(name, "GetEventStatus"))
        return (PFN_vkVoidFunction) __HOOKED_vkGetEventStatus;
    if (!strcmp(name, "SetEvent"))
        return (PFN_vkVoidFunction) __HOOKED_vkSetEvent;
    if (!strcmp(name, "ResetEvent"))
        return (PFN_vkVoidFunction) __HOOKED_vkResetEvent;
    if (!strcmp(name, "CreateQueryPool"))
        return (PFN_vkVoidFunction) __HOOKED_vkCreateQueryPool;
    if (!strcmp(name, "DestroyQueryPool"))
        return (PFN_vkVoidFunction) __HOOKED_vkDestroyQueryPool;
    if (!strcmp(name, "GetQueryPoolResults"))
        return (PFN_vkVoidFunction) __HOOKED_vkGetQueryPoolResults;
    if (!strcmp(name, "CreateBuffer"))
        return (PFN_vkVoidFunction) __HOOKED_vkCreateBuffer;
    if (!strcmp(name, "DestroyBuffer"))
        return (PFN_vkVoidFunction) __HOOKED_vkDestroyBuffer;
    if (!strcmp(name, "CreateBufferView"))
        return (PFN_vkVoidFunction) __HOOKED_vkCreateBufferView;
    if (!strcmp(name, "DestroyBufferView"))
        return (PFN_vkVoidFunction) __HOOKED_vkDestroyBufferView;
    if (!strcmp(name, "CreateImage"))
        return (PFN_vkVoidFunction) __HOOKED_vkCreateImage;
    if (!strcmp(name, "DestroyImage"))
        return (PFN_vkVoidFunction) __HOOKED_vkDestroyImage;
    if (!strcmp(name, "GetImageSubresourceLayout"))
        return (PFN_vkVoidFunction) __HOOKED_vkGetImageSubresourceLayout;
    if (!strcmp(name, "CreateImageView"))
        return (PFN_vkVoidFunction) __HOOKED_vkCreateImageView;
    if (!strcmp(name, "DestroyImageView"))
        return (PFN_vkVoidFunction) __HOOKED_vkDestroyImageView;
    if (!strcmp(name, "CreateShaderModule"))
        return (PFN_vkVoidFunction) __HOOKED_vkCreateShaderModule;
    if (!strcmp(name, "DestroyShaderModule"))
        return (PFN_vkVoidFunction) __HOOKED_vkDestroyShaderModule;
    if (!strcmp(name, "CreateShader"))
        return (PFN_vkVoidFunction) __HOOKED_vkCreateShader;
    if (!strcmp(name, "DestroyShader"))
        return (PFN_vkVoidFunction) __HOOKED_vkDestroyShader;
    if (!strcmp(name, "CreatePipelineCache"))
        return (PFN_vkVoidFunction) __HOOKED_vkCreatePipelineCache;
    if (!strcmp(name, "DestroyPipelineCache"))
        return (PFN_vkVoidFunction) __HOOKED_vkDestroyPipelineCache;
    if (!strcmp(name, "GetPipelineCacheSize"))
        return (PFN_vkVoidFunction) __HOOKED_vkGetPipelineCacheSize;
    if (!strcmp(name, "GetPipelineCacheData"))
        return (PFN_vkVoidFunction) __HOOKED_vkGetPipelineCacheData;
    if (!strcmp(name, "MergePipelineCaches"))
        return (PFN_vkVoidFunction) __HOOKED_vkMergePipelineCaches;
    if (!strcmp(name, "CreateGraphicsPipelines"))
        return (PFN_vkVoidFunction) __HOOKED_vkCreateGraphicsPipelines;
    if (!strcmp(name, "CreateComputePipelines"))
        return (PFN_vkVoidFunction) __HOOKED_vkCreateComputePipelines;
    if (!strcmp(name, "DestroyPipeline"))
        return (PFN_vkVoidFunction) __HOOKED_vkDestroyPipeline;
    if (!strcmp(name, "CreatePipelineLayout"))
        return (PFN_vkVoidFunction) __HOOKED_vkCreatePipelineLayout;
    if (!strcmp(name, "DestroyPipelineLayout"))
        return (PFN_vkVoidFunction) __HOOKED_vkDestroyPipelineLayout;
    if (!strcmp(name, "CreateSampler"))
        return (PFN_vkVoidFunction) __HOOKED_vkCreateSampler;
    if (!strcmp(name, "DestroySampler"))
        return (PFN_vkVoidFunction) __HOOKED_vkDestroySampler;
    if (!strcmp(name, "CreateDescriptorSetLayout"))
        return (PFN_vkVoidFunction) __HOOKED_vkCreateDescriptorSetLayout;
    if (!strcmp(name, "DestroyDescriptorSetLayout"))
        return (PFN_vkVoidFunction) __HOOKED_vkDestroyDescriptorSetLayout;
    if (!strcmp(name, "CreateDescriptorPool"))
        return (PFN_vkVoidFunction) __HOOKED_vkCreateDescriptorPool;
    if (!strcmp(name, "DestroyDescriptorPool"))
        return (PFN_vkVoidFunction) __HOOKED_vkDestroyDescriptorPool;
    if (!strcmp(name, "ResetDescriptorPool"))
        return (PFN_vkVoidFunction) __HOOKED_vkResetDescriptorPool;
    if (!strcmp(name, "AllocDescriptorSets"))
        return (PFN_vkVoidFunction) __HOOKED_vkAllocDescriptorSets;
    if (!strcmp(name, "FreeDescriptorSets"))
        return (PFN_vkVoidFunction) __HOOKED_vkFreeDescriptorSets;
    if (!strcmp(name, "UpdateDescriptorSets"))
        return (PFN_vkVoidFunction) __HOOKED_vkUpdateDescriptorSets;
    if (!strcmp(name, "CreateCommandPool"))
        return (PFN_vkVoidFunction) __HOOKED_vkCreateCommandPool;
    if (!strcmp(name, "DestroyCommandPool"))
        return (PFN_vkVoidFunction) __HOOKED_vkDestroyCommandPool;
    if (!strcmp(name, "ResetCommandPool"))
        return (PFN_vkVoidFunction) __HOOKED_vkResetCommandPool;
    if (!strcmp(name, "CreateCommandBuffer"))
        return (PFN_vkVoidFunction) __HOOKED_vkCreateCommandBuffer;
    if (!strcmp(name, "DestroyCommandBuffer"))
        return (PFN_vkVoidFunction) __HOOKED_vkDestroyCommandBuffer;
    if (!strcmp(name, "BeginCommandBuffer"))
        return (PFN_vkVoidFunction) __HOOKED_vkBeginCommandBuffer;
    if (!strcmp(name, "EndCommandBuffer"))
        return (PFN_vkVoidFunction) __HOOKED_vkEndCommandBuffer;
    if (!strcmp(name, "ResetCommandBuffer"))
        return (PFN_vkVoidFunction) __HOOKED_vkResetCommandBuffer;
    if (!strcmp(name, "CmdBindPipeline"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdBindPipeline;
    if (!strcmp(name, "CmdSetViewport"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdSetViewport;
    if (!strcmp(name, "CmdSetScissor"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdSetScissor;
    if (!strcmp(name, "CmdSetLineWidth"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdSetLineWidth;
    if (!strcmp(name, "CmdSetDepthBias"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdSetDepthBias;
    if (!strcmp(name, "CmdSetBlendConstants"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdSetBlendConstants;
    if (!strcmp(name, "CmdSetDepthBounds"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdSetDepthBounds;
    if (!strcmp(name, "CmdSetStencilCompareMask"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdSetStencilCompareMask;
    if (!strcmp(name, "CmdSetStencilWriteMask"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdSetStencilWriteMask;
    if (!strcmp(name, "CmdSetStencilReference"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdSetStencilReference;
    if (!strcmp(name, "CmdBindDescriptorSets"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdBindDescriptorSets;
    if (!strcmp(name, "CmdBindIndexBuffer"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdBindIndexBuffer;
    if (!strcmp(name, "CmdBindVertexBuffers"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdBindVertexBuffers;
    if (!strcmp(name, "CmdDraw"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdDraw;
    if (!strcmp(name, "CmdDrawIndexed"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdDrawIndexed;
    if (!strcmp(name, "CmdDrawIndirect"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdDrawIndirect;
    if (!strcmp(name, "CmdDrawIndexedIndirect"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdDrawIndexedIndirect;
    if (!strcmp(name, "CmdDispatch"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdDispatch;
    if (!strcmp(name, "CmdDispatchIndirect"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdDispatchIndirect;
    if (!strcmp(name, "CmdCopyBuffer"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdCopyBuffer;
    if (!strcmp(name, "CmdCopyImage"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdCopyImage;
    if (!strcmp(name, "CmdBlitImage"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdBlitImage;
    if (!strcmp(name, "CmdCopyBufferToImage"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdCopyBufferToImage;
    if (!strcmp(name, "CmdCopyImageToBuffer"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdCopyImageToBuffer;
    if (!strcmp(name, "CmdUpdateBuffer"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdUpdateBuffer;
    if (!strcmp(name, "CmdFillBuffer"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdFillBuffer;
    if (!strcmp(name, "CmdClearColorImage"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdClearColorImage;
    if (!strcmp(name, "CmdClearDepthStencilImage"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdClearDepthStencilImage;
    if (!strcmp(name, "CmdClearColorAttachment"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdClearColorAttachment;
    if (!strcmp(name, "CmdClearDepthStencilAttachment"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdClearDepthStencilAttachment;
    if (!strcmp(name, "CmdResolveImage"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdResolveImage;
    if (!strcmp(name, "CmdSetEvent"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdSetEvent;
    if (!strcmp(name, "CmdResetEvent"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdResetEvent;
    if (!strcmp(name, "CmdWaitEvents"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdWaitEvents;
    if (!strcmp(name, "CmdPipelineBarrier"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdPipelineBarrier;
    if (!strcmp(name, "CmdBeginQuery"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdBeginQuery;
    if (!strcmp(name, "CmdEndQuery"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdEndQuery;
    if (!strcmp(name, "CmdResetQueryPool"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdResetQueryPool;
    if (!strcmp(name, "CmdWriteTimestamp"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdWriteTimestamp;
    if (!strcmp(name, "CmdCopyQueryPoolResults"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdCopyQueryPoolResults;
    if (!strcmp(name, "CreateFramebuffer"))
        return (PFN_vkVoidFunction) __HOOKED_vkCreateFramebuffer;
    if (!strcmp(name, "DestroyFramebuffer"))
        return (PFN_vkVoidFunction) __HOOKED_vkDestroyFramebuffer;
    if (!strcmp(name, "CreateRenderPass"))
        return (PFN_vkVoidFunction) __HOOKED_vkCreateRenderPass;
    if (!strcmp(name, "DestroyRenderPass"))
        return (PFN_vkVoidFunction) __HOOKED_vkDestroyRenderPass;
    if (!strcmp(name, "GetRenderAreaGranularity"))
        return (PFN_vkVoidFunction) __HOOKED_vkGetRenderAreaGranularity;
    if (!strcmp(name, "CmdBeginRenderPass"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdBeginRenderPass;
    if (!strcmp(name, "CmdNextSubpass"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdNextSubpass;
    if (!strcmp(name, "CmdPushConstants"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdPushConstants;
    if (!strcmp(name, "CmdEndRenderPass"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdEndRenderPass;
    if (!strcmp(name, "CmdExecuteCommands"))
        return (PFN_vkVoidFunction) __HOOKED_vkCmdExecuteCommands;

    return NULL;
}

static inline PFN_vkVoidFunction layer_intercept_instance_proc(const char *name)
{
    if (!name || name[0] != 'v' || name[1] != 'k')
        return NULL;

    name += 2;
    if (!strcmp(name, "CreateInstance"))
        return (PFN_vkVoidFunction) __HOOKED_vkCreateInstance;
    if (!strcmp(name, "DestroyInstance"))
        return (PFN_vkVoidFunction) __HOOKED_vkDestroyInstance;
    if (!strcmp(name, "EnumeratePhysicalDevices"))
        return (PFN_vkVoidFunction) __HOOKED_vkEnumeratePhysicalDevices;
    if (!strcmp(name, "GetPhysicalDeviceFeatures"))
        return (PFN_vkVoidFunction) __HOOKED_vkGetPhysicalDeviceFeatures;
    if (!strcmp(name, "GetPhysicalDeviceFormatProperties"))
        return (PFN_vkVoidFunction) __HOOKED_vkGetPhysicalDeviceFormatProperties;
    if (!strcmp(name, "GetPhysicalDeviceImageFormatProperties"))
        return (PFN_vkVoidFunction) __HOOKED_vkGetPhysicalDeviceImageFormatProperties;
    if (!strcmp(name, "GetPhysicalDeviceSparseImageFormatProperties"))
        return (PFN_vkVoidFunction) __HOOKED_vkGetPhysicalDeviceSparseImageFormatProperties;
    if (!strcmp(name, "GetPhysicalDeviceProperties"))
        return (PFN_vkVoidFunction) __HOOKED_vkGetPhysicalDeviceProperties;
    if (!strcmp(name, "GetPhysicalDeviceQueueFamilyProperties"))
        return (PFN_vkVoidFunction) __HOOKED_vkGetPhysicalDeviceQueueFamilyProperties;
    if (!strcmp(name, "GetPhysicalDeviceMemoryProperties"))
        return (PFN_vkVoidFunction) __HOOKED_vkGetPhysicalDeviceMemoryProperties;
    if (!strcmp(name, "EnumerateDeviceLayerProperties"))
        return (PFN_vkVoidFunction) __HOOKED_vkEnumerateDeviceLayerProperties;
    if (!strcmp(name, "EnumerateDeviceExtensionProperties"))
        return (PFN_vkVoidFunction) __HOOKED_vkEnumerateDeviceExtensionProperties;

    return NULL;
}

/**
 * Want trace packets created for GetDeviceProcAddr that is app initiated
 * but not for loader initiated calls to GDPA. Thus need two versions of GDPA.
 */
VKTRACER_EXPORT PFN_vkVoidFunction VKAPI vktraceGetDeviceProcAddr(VkDevice device, const char* funcName)
{
    vktrace_trace_packet_header *pHeader;
    PFN_vkVoidFunction addr;
    packet_vkGetDeviceProcAddr* pPacket = NULL;
    CREATE_TRACE_PACKET(vkGetDeviceProcAddr, ((funcName != NULL) ? strlen(funcName) + 1 : 0));
    addr = __HOOKED_vkGetDeviceProcAddr(device, funcName);
    vktrace_set_packet_entrypoint_end_time(pHeader);
    pPacket = interpret_body_as_vkGetDeviceProcAddr(pHeader);
    pPacket->device = device;
    vktrace_add_buffer_to_trace_packet(pHeader, (void**)&(pPacket->pName), ((funcName != NULL) ? strlen(funcName) + 1 : 0), funcName);
    pPacket->result = addr;
    vktrace_finalize_buffer_address(pHeader, (void**)&(pPacket->pName));
    FINISH_TRACE_PACKET();
    return addr;
}

/* GDPA with no trace packet creation */
VKTRACER_EXPORT PFN_vkVoidFunction VKAPI __HOOKED_vkGetDeviceProcAddr(VkDevice device, const char* funcName)
{
    PFN_vkVoidFunction addr;
    if (device == VK_NULL_HANDLE) {
        return NULL;
    }

    /* loader uses this to force layer initialization; device object is wrapped */
    if (!strcmp("vkGetDeviceProcAddr", funcName)) {
        initDeviceData(g_deviceDataMap, (VkBaseLayerObject *) device);
        if (gMessageStream != NULL) {
            return (PFN_vkVoidFunction) vktraceGetDeviceProcAddr;
        } else {
            return (PFN_vkVoidFunction) __HOOKED_vkGetDeviceProcAddr;
        }
    }


    layer_device_data  *devData = mdd(device);
    if (gMessageStream != NULL) {

        addr = layer_intercept_proc(funcName);
        if (addr)
            return addr;

        if (devData->KHRDeviceSwapchainEnabled)
        {
            if (!strcmp("vkGetSurfacePropertiesKHR", funcName))
                return (PFN_vkVoidFunction) __HOOKED_vkGetSurfacePropertiesKHR;
            if (!strcmp("vkGetSurfaceFormatsKHR", funcName))
                return (PFN_vkVoidFunction) __HOOKED_vkGetSurfaceFormatsKHR;
            if (!strcmp("vkGetSurfacePresentModesKHR", funcName))
                return (PFN_vkVoidFunction) __HOOKED_vkGetSurfacePresentModesKHR;
            if (!strcmp("vkCreateSwapchainKHR", funcName))
                return (PFN_vkVoidFunction) __HOOKED_vkCreateSwapchainKHR;
            if (!strcmp("vkDestroySwapchainKHR", funcName))
                return (PFN_vkVoidFunction) __HOOKED_vkDestroySwapchainKHR;
            if (!strcmp("vkGetSwapchainImagesKHR", funcName))
                return (PFN_vkVoidFunction) __HOOKED_vkGetSwapchainImagesKHR;
            if (!strcmp("vkAcquireNextImageKHR", funcName))
                return (PFN_vkVoidFunction) __HOOKED_vkAcquireNextImageKHR;
            if (!strcmp("vkQueuePresentKHR", funcName))
                return (PFN_vkVoidFunction) __HOOKED_vkQueuePresentKHR;
        }
        if (devData->LunargDebugMarkerEnabled)
        {
            if (!strcmp("vkCmdDbgMarkerBegin", funcName))
                return (PFN_vkVoidFunction) __HOOKED_vkCmdDbgMarkerBegin;
            if (!strcmp("vkCmdDbgMarkerEnd", funcName))
                return (PFN_vkVoidFunction) __HOOKED_vkCmdDbgMarkerEnd;
            if (!strcmp("vkDbgSetObjectTag", funcName))
                return (PFN_vkVoidFunction) __HOOKED_vkDbgSetObjectTag;
            if (!strcmp("vkDbgSetObjectName", funcName))
                return (PFN_vkVoidFunction) __HOOKED_vkDbgSetObjectName;
        }
    }
    VkLayerDispatchTable *pDisp =  &devData->devTable;
    if (pDisp->GetDeviceProcAddr == NULL)
        return NULL;
    return pDisp->GetDeviceProcAddr(device, funcName);
}

/**
 * Want trace packets created for GetInstanceProcAddr that is app initiated
 * but not for loader initiated calls to GIPA. Thus need two versions of GIPA.
 */
VKTRACER_EXPORT PFN_vkVoidFunction VKAPI vktraceGetInstanceProcAddr(VkInstance instance, const char* funcName)
{
    vktrace_trace_packet_header* pHeader;
    PFN_vkVoidFunction addr;
    packet_vkGetInstanceProcAddr* pPacket = NULL;
    assert(strcmp("vkGetInstanceProcAddr", funcName));
    CREATE_TRACE_PACKET(vkGetInstanceProcAddr, ((funcName != NULL) ? strlen(funcName) + 1 : 0));
    addr = __HOOKED_vkGetInstanceProcAddr(instance, funcName);
    vktrace_set_packet_entrypoint_end_time(pHeader);
    pPacket = interpret_body_as_vkGetInstanceProcAddr(pHeader);
    pPacket->instance = instance;
    vktrace_add_buffer_to_trace_packet(pHeader, (void**) &(pPacket->pName), ((funcName != NULL) ? strlen(funcName) + 1 : 0), funcName);
    pPacket->result = addr;
    vktrace_finalize_buffer_address(pHeader, (void**) &(pPacket->pName));
    FINISH_TRACE_PACKET();
    return addr;
}

/* GIPA with no trace packet creation */
VKTRACER_EXPORT PFN_vkVoidFunction VKAPI __HOOKED_vkGetInstanceProcAddr(VkInstance instance, const char* funcName)
{
    PFN_vkVoidFunction addr;
    if (instance == VK_NULL_HANDLE) {
        return NULL;
    }

    vktrace_platform_thread_once(&gInitOnce, InitTracer);

    /* loader uses this to force layer initialization; instance object is wrapped */
    if (!strcmp("vkGetInstanceProcAddr", funcName)) {
        initInstanceData(g_instanceDataMap, (VkBaseLayerObject *) instance);
        if (gMessageStream != NULL) {
            return (PFN_vkVoidFunction) vktraceGetInstanceProcAddr;
        } else {
            return (PFN_vkVoidFunction) __HOOKED_vkGetInstanceProcAddr;
        }
    }

    layer_instance_data  *instData = mid(instance);
    if (gMessageStream != NULL) {

        addr = layer_intercept_instance_proc(funcName);
        if (addr)
            return addr;

        if (instData->LunargDebugReportEnabled)
        {
            if (!strcmp("vkDbgCreateMsgCallback", funcName))
                return (PFN_vkVoidFunction) __HOOKED_vkDbgCreateMsgCallback;
            if (!strcmp("vkDbgDestroyMsgCallback", funcName))
                return (PFN_vkVoidFunction) __HOOKED_vkDbgDestroyMsgCallback;

        }
        if (instData->KHRSwapchainEnabled)
        {
            if (!strcmp("vkGetPhysicalDeviceSurfaceSupportKHR", funcName))
                return (PFN_vkVoidFunction) __HOOKED_vkGetPhysicalDeviceSurfaceSupportKHR;
        }
    }

    VkLayerInstanceDispatchTable* pTable = &instData->instTable;
    if (pTable->GetInstanceProcAddr == NULL)
        return NULL;

    return pTable->GetInstanceProcAddr(instance, funcName);
}