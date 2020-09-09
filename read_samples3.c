/*
 *  read_samples3.c
 * 
 
 Program to read mef format file (v3.0) and output samples/timestamps to standard out.
 
 Copyright 2020, Mayo Foundation, Rochester MN. All rights reserved.
 
 This software is made freely available under the GNU public license: http://www.gnu.org/licenses/gpl-3.0.txt
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "meflib.h"

MEF_GLOBALS	*MEF_globals;

int check_block_crc(ui1* block_hdr_ptr, ui4 max_samps, ui1* total_data_ptr, ui8 total_data_bytes)
{
    ui8 offset_into_data, remaining_buf_size;
    si1 CRC_valid;
    RED_BLOCK_HEADER* block_header;
    
    offset_into_data = block_hdr_ptr - total_data_ptr;
    remaining_buf_size = total_data_bytes - offset_into_data;
    
    // check if remaining buffer at least contains the RED block header
    if (remaining_buf_size < RED_BLOCK_HEADER_BYTES)
        return 0;
    
    block_header = (RED_BLOCK_HEADER*) block_hdr_ptr;
    
    // check if entire block, based on size specified in header, can possibly fit in the remaining buffer
    if (block_header->block_bytes > remaining_buf_size)
        return 0;
    
    // check if size specified in header is absurdly large
    if (block_header->block_bytes > RED_MAX_COMPRESSED_BYTES(max_samps, 1))
        return 0;
    
    // at this point we know we have enough data to actually run the CRC calculation, so do it
    CRC_valid = CRC_validate((ui1*) block_header + CRC_BYTES, block_header->block_bytes - CRC_BYTES, block_header->block_CRC);
    
    // return output of CRC heck
    if (CRC_valid == MEF_TRUE)
        return 1;
    else
        return 0;
}

int main (int argc, const char * argv[]) {
    si4 i, numBlocks, start_block;
    si4 *data;
    ui8 inDataLength, outDataLength;
    ui1 *in_data;
    si4 numSegments, start_segment;
    si8 temp_time;
    
    CHANNEL    *channel;
    RED_PROCESSING_STRUCT	*rps;
    ui4			max_samps;
    FILE *fp;
    si4 n_read;
    
    (void) initialize_meflib();
    
    if (argc < 2 || argc > 3)
    {
        (void) printf("USAGE: %s channel_name [password] \n", argv[0]);
        return(1);
    }
    
    if (argc == 3)
    {
        // check input arguments for password
        channel = read_MEF_channel(NULL, argv[1], TIME_SERIES_CHANNEL_TYPE, argv[2], NULL, MEF_FALSE, MEF_FALSE);
    }
    else
        channel = read_MEF_channel(NULL, argv[1], TIME_SERIES_CHANNEL_TYPE, NULL, NULL, MEF_FALSE, MEF_FALSE);
    
    inDataLength = channel->metadata.time_series_section_2->maximum_block_bytes;
    in_data = malloc(inDataLength);
    outDataLength = max_samps = channel->metadata.time_series_section_2->maximum_block_samples;
    data = calloc(outDataLength, sizeof(ui4));
    
    // create RED processing struct
    rps = (RED_PROCESSING_STRUCT *) calloc((size_t) 1, sizeof(RED_PROCESSING_STRUCT));
    rps->compression.mode = RED_DECOMPRESSION;
    //rps->directives.return_block_extrema = MEF_TRUE;
    rps->decompressed_data = in_data;
    rps->difference_buffer = (si1 *) e_calloc((size_t) RED_MAX_DIFFERENCE_BYTES(max_samps), sizeof(ui1), __FUNCTION__, __LINE__, USE_GLOBAL_BEHAVIOR);
    
    // error checking
    if (channel == NULL) {
        fprintf(stdout, "Error opening channel\n");
        return (0);
    }
    
    numSegments = channel->number_of_segments;
    start_segment = 0;
    
    fprintf(stdout, "\n\nReading and decompressing channel %s, segments = %d \n", argv[1], numSegments);
    
    // iterate over segments
    while (start_segment < numSegments) {

        if (channel->segments[start_segment].time_series_data_fps->fp == NULL) {
            channel->segments[start_segment].time_series_data_fps->fp = fopen(channel->segments[start_segment].time_series_data_fps->full_file_name, "rb");
#ifndef _WIN32
            channel->segments[start_segment].time_series_data_fps->fd = fileno(channel->segments[start_segment].time_series_data_fps->fp);
#else
            channel->segments[start_segment].time_series_data_fps->fd = _fileno(channel->segments[start_segment].time_series_data_fps->fp);
#endif
        }
        
        numBlocks = channel->segments[start_segment].time_series_indices_fps->universal_header->number_of_entries;
        
        temp_time = channel->segments[start_segment].time_series_data_fps->universal_header->start_time;
        remove_recording_time_offset(&temp_time);
        
        fprintf(stdout, "\nNew Segment, segment %d: samples = %ld, blocks = %ld, rate = %d time = %ld \n",
                start_segment,
                channel->segments[start_segment].metadata_fps->metadata.time_series_section_2->number_of_samples,
                numBlocks,
                (int)(channel->segments[start_segment].metadata_fps->metadata.time_series_section_2->sampling_frequency),
                temp_time);
        
        start_block = 0;
        
        // iterate over blocks within a segment
        while( start_block < numBlocks ) {
            
            fp = channel->segments[start_segment].time_series_data_fps->fp;
#ifndef _WIN32
            fseek(fp, channel->segments[start_segment].time_series_indices_fps->time_series_indices[start_block].file_offset, SEEK_SET);
#else
            _fseeki64(fp, channel->segments[start_segment].time_series_indices_fps->time_series_indices[start_block].file_offset, SEEK_SET);
#endif
            n_read = fread(in_data, sizeof(si1), (size_t) channel->segments[start_segment].time_series_indices_fps->time_series_indices[start_block].block_bytes, fp);
            
            rps->compressed_data = in_data;
            rps->decompressed_ptr = data;
            rps->block_header = (RED_BLOCK_HEADER *) rps->compressed_data;
            if (!check_block_crc((ui1*)(rps->block_header), max_samps, in_data, inDataLength))
            {
                fprintf(stdout, "**CRC block failure!**\n");
                start_block++;
                continue;
            }

            RED_decode(rps);
            
            fprintf(stdout, "\nNew Block, size = %d time = %lu\n\n",
                    rps->block_header->number_of_samples,
                    rps->block_header->start_time);
            
            for (i=0;i<rps->block_header->number_of_samples;i++)
                fprintf(stdout, "%d\n", data[i]);
            
            start_block++;
        }

        if (channel->segments[start_segment].time_series_data_fps->fp != NULL)
            fclose(channel->segments[start_segment].time_series_data_fps->fp);
        
        start_segment++;
    }
    
    // clean up
    free(in_data);
    free(data);
    free(rps);
    
    fprintf(stdout, "Decompression complete\n");
    
    return 0;
}
