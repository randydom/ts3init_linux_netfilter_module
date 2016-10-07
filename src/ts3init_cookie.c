/*
 *    "ts3init" extension for Xtables
 *
 *    Description: A module to aid in ts3 spoof protection
 *                   This is the "cookie" related code
 *
 *    Authors:
 *    Niels Werensteijn <niels werensteijn [at] teampseak com>, 2016-10-03
 *
 *    This program is free software; you can redistribute it and/or modify it
 *    under the terms of the GNU General Public License; either version 2
 *    or 3 of the License, as published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/crypto.h>
#include <linux/err.h>
#include <linux/scatterlist.h>
#include "ts3init_cookie.h"

static void check_update_seed_cache(time_t time, __u8 index, 
                struct xt_ts3init_cookie_cache* cache,
                const __u8* cookie_seed)
{
    struct hash_desc desc;
    struct scatterlist sg[2];
    int ret;
    __le32 seed_hash_time;

    if (time == cache->time[index]) return;

    /* We need to update the cache. */
    /* seed = sha512(cookie_seed[60] + __le32 time) */
    seed_hash_time = cpu_to_le32( (__u32)time);
    sg_init_table(sg, ARRAY_SIZE(sg));
    sg_set_buf(&sg[0], cookie_seed, 60);
    sg_set_buf(&sg[1], &seed_hash_time, 4);

    desc.tfm = crypto_alloc_hash("sha512", 0, 0);
    desc.flags = 0;

    if (IS_ERR(desc.tfm))
    {
        printk(KERN_ERR KBUILD_MODNAME ": could not alloc sha512\n");
        return;
    }

    ret = crypto_hash_init(&desc);
    if (ret != 0)
    {
        printk(KERN_ERR KBUILD_MODNAME ": could not initalize sha512\n");
        return;
    }

    ret = crypto_hash_digest(&desc, sg, 64, cache->seed + index * SHA512_SIZE);
    if (ret != 0)
    {
        printk(KERN_ERR KBUILD_MODNAME ": could not digest sha512\n");
        return;
    }

    crypto_free_hash(desc.tfm);
}

static __u8* get_cookie_seed(time_t current_time, __u8 packet_index, 
                struct xt_ts3init_cookie_cache* cache,
                const __u8* cookie_seed)
{

    __u8 current_cache_index;
    __u8 packet_cache_index;
    time_t current_cache_time;
    time_t packet_cache_time;

    if (packet_index >= 8) return NULL;
    
    current_cache_index = (current_time >> 2) & 1;
    packet_cache_index = packet_index >> 2 /* &1 */;

    /* get cache time of packet */
    current_cache_time = current_time & ~((time_t)3);
    packet_cache_time = current_cache_index == packet_cache_index ?
        current_cache_time : current_cache_time - (1*4);

    /* make sure the cache is up-to-date */
    check_update_seed_cache(packet_cache_time, packet_cache_index, cache,
        cookie_seed);

    /* return the proper seed */
    return cache->seed + (SIP_KEY_SIZE * packet_index );
}
