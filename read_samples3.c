/*

 *  read_samples3.c
 * 
 
 Program to read mef format file (v3.0) and output samples/timestamps to standard out.
 
 Copyright 2019, Mayo Foundation, Rochester MN. All rights reserved.
 
 This software is made freely available under the GNU public license: http://www.gnu.org/licenses/gpl-3.0.txt
 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "meflib.h"

MEF_GLOBALS	*MEF_globals;

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
    max_samps = channel->metadata.time_series_section_2->maximum_block_samples;
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
        
        numBlocks = channel->segments[start_segment].time_series_indices_fps->universal_header->number_of_entries;
        
        temp_time = channel->segments[start_segment].time_series_data_fps->universal_header->start_time;
        remove_recording_time_offset(&temp_time);
        
        fprintf(stdout, "\nNew Segment, segment %d: samples = %ld, blocks = %ld, time = %ld \n",
                start_segment,
                channel->segments[start_segment].metadata_fps->metadata.time_series_section_2->number_of_samples,
                numBlocks,
                temp_time);
        
        start_block = 0;
        
        // iterate over blocks within a segment
        while( start_block < numBlocks ) {
            
            fp = channel->segments[start_segment].time_series_data_fps->fp;
            fseek(fp, channel->segments[start_segment].time_series_indices_fps->time_series_indices[start_block].file_offset, SEEK_SET);
            n_read = fread(in_data, sizeof(si1), (size_t) channel->segments[start_segment].time_series_indices_fps->time_series_indices[start_block].block_bytes, fp);
            
            rps->compressed_data = in_data;
            rps->decompressed_ptr = data;
            rps->block_header = (RED_BLOCK_HEADER *) rps->compressed_data;
            RED_decode(rps);
            
            fprintf(stdout, "\nNew Block, size = %d time = %lu\n\n",
                    rps->block_header->number_of_samples,
                    rps->block_header->start_time);
            
            for (i=0;i<rps->block_header->number_of_samples;i++)
                fprintf(stdout, "%d\n", data[i]);
            
            start_block++;
        }
        
        start_segment++;
    }
    
    // clean up
    free(in_data);
    free(data);
    free(rps);
    
    fprintf(stdout, "Decompression complete\n");
    
    return 0;
}
