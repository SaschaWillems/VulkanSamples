/*
 * Vulkan Samples Kit
 *
 * Copyright (C) 2015 LunarG, Inc.
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

/*
VULKAN_SAMPLE_SHORT_DESCRIPTION
Inititalize Texture
*/

#include <util_init.hpp>
#include <assert.h>
#include <cstdlib>

int main(int argc, char **argv)
{
    VkResult res;
    struct sample_info info = {};
    char sample_title[] = "Texture Initialization Sample";

    init_global_layer_properties(info);
    info.instance_extension_names.push_back(VK_WSI_SWAPCHAIN_EXTENSION_NAME);
    info.device_extension_names.push_back(VK_WSI_DEVICE_SWAPCHAIN_EXTENSION_NAME);
    init_instance(info, sample_title);
    init_enumerate_device(info);
    init_device(info);
    init_connection(info);
    info.width = info.height = 50;
    init_window(info);
    init_wsi(info);
    init_and_begin_command_buffer(info);
    init_device_queue(info);

    /* VULKAN_KEY_START */
    /*
     * Set up textures:
     * - Create a linear tiled image
     * - Map it and write the texture data into it
     * - If linear images cannot be used as textures, create an optimally
     *       tiled image and blit from the linearly tiled image to the optimally
     *       tiled image
     * -
     * -
     * -
     */

    struct texture_object texObj;
    std::string filename = get_base_data_dir();
    filename.append("lunarg.ppm");
    if (!read_ppm(filename.c_str(), &texObj.tex_width, &texObj.tex_height, 0, NULL))
    {
        std::cout << "Could not read texture file lunarg.ppm\n";
        exit(-1);
    }

    VkFormatProperties formatProps;
    res = vkGetPhysicalDeviceFormatProperties(info.gpu, VK_FORMAT_R8G8B8A8_UNORM, &formatProps);
    assert(!res);

    /* See if we can use a linear tiled image for a texture, if not, we will need a staging image for the texture data */
    bool needStaging = (!(formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))?true:false;

    VkImageCreateInfo image_create_info = {};
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.pNext = NULL;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    image_create_info.extent.width = texObj.tex_width;
    image_create_info.extent.height = texObj.tex_height;
    image_create_info.extent.depth = 1;
    image_create_info.mipLevels = 1;
    image_create_info.arraySize = 1;
    image_create_info.samples = 1;
    image_create_info.tiling = VK_IMAGE_TILING_LINEAR;
    image_create_info.usage = needStaging?VK_IMAGE_USAGE_TRANSFER_SOURCE_BIT:
                                          VK_IMAGE_USAGE_SAMPLED_BIT;
    image_create_info.queueFamilyCount = 0;
    image_create_info.pQueueFamilyIndices = NULL;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_create_info.flags = 0;

    VkMemoryAllocInfo mem_alloc = {};
    mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO;
    mem_alloc.pNext = NULL;
    mem_alloc.allocationSize = 0;
    mem_alloc.memoryTypeIndex = 0;

    VkImage mappableImage;
    VkDeviceMemory mappableMemory;

    VkMemoryRequirements mem_reqs;

    /* Create a mappable image.  It will be the texture if linear images are ok to be textures */
    /* or it will be the staging image if they are not.                                        */
    res = vkCreateImage(info.device, &image_create_info,
            &mappableImage);
    assert(!res);

    res = vkGetImageMemoryRequirements(info.device, mappableImage, &mem_reqs);
    assert(!res);

    mem_alloc.allocationSize = mem_reqs.size;

    /* Find the memory type that is host mappable */
    res = memory_type_from_properties(info, mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &mem_alloc.memoryTypeIndex);
    assert(!res);

    /* allocate memory */
    res = vkAllocMemory(info.device, &mem_alloc,
                &(mappableMemory));
    assert(!res);

    /* bind memory */
    res = vkBindImageMemory(info.device, mappableImage,
            mappableMemory, 0);
    assert(!res);

    VkImageSubresource subres = {};
    subres.aspect = VK_IMAGE_ASPECT_COLOR;
    subres.mipLevel = 0;
    subres.arraySlice = 0;

    VkSubresourceLayout layout;
    void *data;

    /* Get the subresource layout so we know what the row pitch is */
    res = vkGetImageSubresourceLayout(info.device, mappableImage, &subres, &layout);
    assert(!res);

    res = vkMapMemory(info.device, mappableMemory, 0, 0, 0, &data);
    assert(!res);

    /* Read the ppm file into the mappable image's memory */
    if (!read_ppm(filename.c_str(), &texObj.tex_width, &texObj.tex_height, layout.rowPitch, (char *)data)) {
        std::cout << "Could not load texture file lunarg.ppm\n";
        exit(-1);
    }

    res = vkUnmapMemory(info.device, mappableMemory);
    assert(!res);

    if (!needStaging) {
        /* If we can use the linear tiled image as a texture, just do it */
        texObj.image = mappableImage;
        texObj.mem = mappableMemory;
        texObj.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        set_image_layout(info, texObj.image,
                               VK_IMAGE_ASPECT_COLOR,
                               VK_IMAGE_LAYOUT_UNDEFINED,
                               texObj.imageLayout);
    } else {
        /* The mappable image cannot be our texture, so create an optimally tiled image and blit to it */
        image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_create_info.usage = VK_IMAGE_USAGE_TRANSFER_DESTINATION_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        res = vkCreateImage(info.device, &image_create_info,
                &texObj.image);
        assert(!res);

        res = vkGetImageMemoryRequirements(info.device, texObj.image, &mem_reqs);
        assert(!res);

        mem_alloc.allocationSize = mem_reqs.size;

        /* Find memory type with DEVICE_ONLY property */
        res = memory_type_from_properties(info, mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_ONLY, &mem_alloc.memoryTypeIndex);
        assert(!res);

        /* allocate memory */
        res = vkAllocMemory(info.device, &mem_alloc,
                    &texObj.mem);
        assert(!res);

        /* bind memory */
        res = vkBindImageMemory(info.device, texObj.image,
                texObj.mem, 0);
        assert(!res);

        /* Since we're going to blit from the mappable image, set its layout to SOURCE_OPTIMAL */
        /* Side effect is that this will create info.cmd                                       */
        set_image_layout(info, mappableImage,
                          VK_IMAGE_ASPECT_COLOR,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL);

        /* Since we're going to blit to the texture image, set its layout to DESTINATION_OPTIMAL */
        set_image_layout(info, texObj.image,
                          VK_IMAGE_ASPECT_COLOR,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL);

        VkImageCopy copy_region;
        copy_region.srcSubresource.arraySlice = 0;
        copy_region.srcSubresource.mipLevel = 0;
        copy_region.srcOffset.x = 0;
        copy_region.srcOffset.y = 0;
        copy_region.srcOffset.z = 0;
        copy_region.destSubresource.aspect = VK_IMAGE_ASPECT_COLOR;
        copy_region.destSubresource.arraySlice = 0;
        copy_region.destSubresource.mipLevel = 0;
        copy_region.destOffset.x = 0;
        copy_region.destOffset.y = 0;
        copy_region.destOffset.z = 0;
        copy_region.extent.width = texObj.tex_width;
        copy_region.extent.height = texObj.tex_height;
        copy_region.extent.depth = 1;

        /* Put the copy command into the command buffer */
        vkCmdCopyImage(info.cmd,
                        mappableImage, VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL,
                        texObj.image, VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL,
                        1, &copy_region);

        /* Set the layout for the texture image from DESTINATION_OPTIMAL to SHADER_READ_ONLY */
        texObj.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        set_image_layout(info, texObj.image,
                               VK_IMAGE_ASPECT_COLOR,
                               VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL,
                               texObj.imageLayout);

        /* Release the resources for the staging image */
        vkFreeMemory(info.device, mappableMemory);
        vkDestroyImage(info.device, mappableImage);
    }
    end_and_submit_command_buffer(info);

    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_TEX_FILTER_NEAREST;
    samplerCreateInfo.minFilter = VK_TEX_FILTER_NEAREST;
    samplerCreateInfo.mipMode = VK_TEX_MIPMAP_MODE_BASE;
    samplerCreateInfo.addressU = VK_TEX_ADDRESS_WRAP;
    samplerCreateInfo.addressV = VK_TEX_ADDRESS_WRAP;
    samplerCreateInfo.addressW = VK_TEX_ADDRESS_WRAP;
    samplerCreateInfo.mipLodBias = 0.0;
    samplerCreateInfo.maxAnisotropy = 0;
    samplerCreateInfo.compareEnable = VK_FALSE;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod = 0.0;
    samplerCreateInfo.maxLod = 0.0;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    /* create sampler */
    res = vkCreateSampler(info.device, &samplerCreateInfo,
            &texObj.sampler);
    assert(!res);

    VkImageViewCreateInfo view_info = {};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.pNext = NULL;
    view_info.image = VK_NULL_HANDLE;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    view_info.channels.r = VK_CHANNEL_SWIZZLE_R;
    view_info.channels.g = VK_CHANNEL_SWIZZLE_G;
    view_info.channels.b = VK_CHANNEL_SWIZZLE_B;
    view_info.channels.a = VK_CHANNEL_SWIZZLE_A;
    view_info.subresourceRange.aspect = VK_IMAGE_ASPECT_COLOR;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.mipLevels = 1;
    view_info.subresourceRange.baseArraySlice = 0;
    view_info.subresourceRange.arraySize = 1;

    /* create image view */
    view_info.image = texObj.image;
    res = vkCreateImageView(info.device, &view_info,
            &texObj.view);
    assert(!res);

    info.textures.push_back(texObj);
    /* VULKAN_KEY_END */

    /* Clean Up */
    vkDestroyCommandBuffer(info.device, info.cmd);
    vkDestroyCommandPool(info.device, info.cmd_pool);
    vkDestroySampler(info.device, texObj.sampler);
    vkFreeMemory(info.device, texObj.mem);
    vkDestroyImageView(info.device, texObj.view);
    vkDestroyImage(info.device, texObj.image);
    vkDestroyDevice(info.device);
    vkDestroyInstance(info.inst);
    destroy_window(info);
}