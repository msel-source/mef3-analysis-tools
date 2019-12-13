/*
 
 *  check_mef3.c
 *
 
 Program to validate a MEF 3 channel.  It looks for inconsistencies in MEF 3 channels.
 
 This program is an update of the previous (MEF version 2) checker.
 
 Copyright 2019, Mayo Foundation, Rochester MN. All rights reserved.
 
 This software is made freely available under the GNU public license: http://www.gnu.org/licenses/gpl-3.0.txt
 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "meflib.h"

MEF_GLOBALS	*MEF_globals;


int validate_mef3(char *channelname, char *log_filename, char *password)
{
    int i, blocks_per_read;
    ui1 logfile, bad_index;
    ui8 num_errors;
    ui8 errors_before_this_segment;
    CHANNEL *channel;
    FILE *lfp;
    si4 start_segment, numSegments;
    char message[200], *time_str;
    time_t now;
    si4 numBlocks;
    si8 temp_time, temp_time2;
    si4 start_block;
    FILE *fp;
    si4 n_read;
    ui1 in_data;
    RED_PROCESSING_STRUCT *rps;
    si4 *data;
    si8 calc_end_time;
    si8 offset;
    si8 dt, ds;
    ui8 data_start, data_end;
    ui4 crc;
    ui8 n;
    ui4 block_size;
    ui8 uh_start, uh_end;
    si8 number_of_blocks;
    
    (void) initialize_meflib();
    
    MEF_globals->CRC_mode = 2;
    
    blocks_per_read = 300;
    num_errors = 0;
    bad_index = 0;
    
    
    if (channelname == NULL) {
        fprintf(stdout, "[%s] Error: NULL mef filename pointer passed in\n", __FUNCTION__);
        return(1);
    }
    
    //NULL or empty log_filename directs output to stdout only
    if ((log_filename == NULL)||(*log_filename==0)) {
        lfp = NULL;
        logfile = 0;
    }
    else {
        //check to see if log file exists
        logfile = 1;
        lfp = fopen(log_filename, "r");
        if (lfp != NULL)
            fprintf(stdout, "[%s] Appending to existing logfile %s\n", __FUNCTION__, log_filename);
        fclose(lfp);
        lfp = fopen(log_filename, "a+");
        if (lfp == NULL) {
            fprintf(stdout, "[%s] Error opening %s for writing\n", __FUNCTION__, log_filename);
            return(1);
        }
    }
    
    now = time(NULL);
    time_str = ctime(&now); time_str[24]=0;
    
    sprintf(message, "\n%s: Beginning MEF validation check of file %s\n", time_str, channelname);
    fprintf(stdout, "%s", message);
    if (logfile) fprintf(lfp, "%s", message);
    
    fprintf(stdout, "\n- Checking header CRCs for all files, and body CRCs for metadata and index files:\n\n");
    
    if (password == NULL)
        channel = read_MEF_channel(NULL, channelname, TIME_SERIES_CHANNEL_TYPE, NULL, NULL, MEF_FALSE, MEF_FALSE);
    else
        channel = read_MEF_channel(NULL, channelname, TIME_SERIES_CHANNEL_TYPE, password, NULL, MEF_FALSE, MEF_FALSE);
    
    //fprintf(stdout, " number of blocks = %ld\n", channel->segments[1].time_series_indices_fps->universal_header->number_of_entries);
    //fprintf(stdout, " number of blocks = %ld\n",  channel->segments[1].metadata_fps->metadata.time_series_section_2->number_of_blocks);
    //fprintf(stdout, " number of samples = %ld\n", channel->segments[1].metadata_fps->metadata.time_series_section_2->number_of_samples);
    //fprintf(stdout, " max block bytes = %ld\n", channel->segments[1].metadata_fps->metadata.time_series_section_2->maximum_block_bytes);
    
    data = calloc(channel->metadata.time_series_section_2->sampling_frequency * (channel->metadata.time_series_section_2->block_interval / 1e6), blocks_per_read * 10);
    //fprintf(stdout, "allocating: %f\n", channel->metadata.time_series_section_2->sampling_frequency * blocks_per_read * 10);
    
    if (channel == NULL)
    {
        fprintf(stdout, "[%s] Error with read_MEF_channel() for %s, returned NULL\n", __FUNCTION__, channelname);
        return(1);
    }

    
    //// Begin checking mef file ///
    //Check header recording times against index array
    temp_time = channel->segments[0].metadata_fps->universal_header->start_time;
    remove_recording_time_offset(&temp_time);
    if (temp_time != channel->segments[0].time_series_indices_fps->time_series_indices[0].start_time) {
        num_errors++;
        sprintf(message, "Metadata header start_time %ld does not match index array time %ld\n",
                channel->earliest_start_time,
                channel->segments[0].time_series_indices_fps->time_series_indices[0].start_time);
        fprintf(stdout, "%s", message);
        if (logfile) fprintf(lfp, "%s", message);
    }
    
    temp_time2 = channel->latest_end_time;
    remove_recording_time_offset(&temp_time2);
    
    calc_end_time = temp_time +
    (ui8)(0.5 + 1000000.0 * (sf8)channel->metadata.time_series_section_2->number_of_samples/channel->metadata.time_series_section_2->sampling_frequency);
    
    if (temp_time2 < calc_end_time) {
        num_errors++;
        
        sprintf(message, "Channel latest_end_time %ld does not match sampling freqency and number of samples\n",
                temp_time2);
        fprintf(stdout, "%s", message);
        if (logfile) fprintf(lfp, "%s", message);
    }
    
    
    numSegments = channel->number_of_segments;
    start_segment = 0;
    
    
    if (numSegments == 0)
    {fprintf(stdout, "[%s] number of segments is zero, must have at least one segmnent for channel %s\n", __FUNCTION__, channelname); return(1); }
    
    fprintf(stdout, "\n");
    
    // ************************
    // Iterate over segments, checking indices
    // ************************
    
    while (start_segment < numSegments)
    {
        
        fprintf(stdout, "- Examining index of segment %s\n", channel->segments[start_segment].name);
        
        // check for overlap between segments in either time or sample number
        if (start_segment > 0)
        {
            // check for sample overlap between segments
            if (channel->segments[start_segment].metadata_fps->metadata.time_series_section_2->start_sample <
                (channel->segments[start_segment-1].metadata_fps->metadata.time_series_section_2->start_sample +
                 channel->segments[start_segment-1].metadata_fps->metadata.time_series_section_2->number_of_samples))
            {
                num_errors++;
                sprintf(message, "Overlap in samples between segments %s and %s, start_sample of segment %s is %ld, start_sample of segment %s is %ld, number_of_samples in %d is %ld\n",
                        channel->segments[start_segment-1].name, channel->segments[start_segment].name, channel->segments[start_segment].name, channel->segments[start_segment].metadata_fps->metadata.time_series_section_2->start_sample,
                        channel->segments[start_segment-1].name, channel->segments[start_segment-1].metadata_fps->metadata.time_series_section_2->start_sample,
                        channel->segments[start_segment-1].name, channel->segments[start_segment-1].metadata_fps->metadata.time_series_section_2->number_of_samples);
                fprintf(stdout, "%s", message);
                if (logfile) fprintf(lfp, "%s", message);
            }
            
            uh_start = channel->segments[start_segment].metadata_fps->universal_header->start_time;
            remove_recording_time_offset(&uh_start);
            uh_end = channel->segments[start_segment-1].metadata_fps->universal_header->end_time;
            remove_recording_time_offset(&uh_end);
            
            // check for time overlap
            // check for sample overlap between segments
            if (uh_start < uh_end)
            {
                num_errors++;
                sprintf(message, "Overlap in time between segments %s and %s, start_time of segment of %d is %ld, end_time of segment %s is %ld\n",
                        channel->segments[start_segment-1].name, channel->segments[start_segment].name, start_segment, uh_start,
                        channel->segments[start_segment-1].name, uh_end);
                fprintf(stdout, "%s", message);
                if (logfile) fprintf(lfp, "%s", message);
            }
        }
        
        //fprintf(stdout, " number of blocks = %ld\n", channel->segments[start_segment].time_series_indices_fps->universal_header->number_of_entries);
        //fprintf(stdout, " number of blocks = %ld\n",  channel->segments[start_segment].metadata_fps->metadata.time_series_section_2->number_of_blocks);
        //fprintf(stdout, " number of samples = %ld\n", channel->segments[start_segment].metadata_fps->metadata.time_series_section_2->number_of_samples);
        //fprintf(stdout, " max block bytes = %ld\n", channel->segments[start_segment].metadata_fps->metadata.time_series_section_2->maximum_block_bytes);
        number_of_blocks = channel->segments[start_segment].time_series_indices_fps->universal_header->number_of_entries;
        number_of_blocks = channel->segments[start_segment].metadata_fps->metadata.time_series_section_2->number_of_blocks;
        
        if (channel->segments[start_segment].time_series_indices_fps->universal_header->number_of_entries !=
            channel->segments[start_segment].metadata_fps->metadata.time_series_section_2->number_of_blocks)
        {
            num_errors++;
            sprintf(message, "Number_of_blocks in metadata %ld does not match number_of_blocks in header of data file %ld in segment %s\n",
                    channel->segments[start_segment].metadata_fps->metadata.time_series_section_2->number_of_blocks,
                    channel->segments[start_segment].time_series_indices_fps->universal_header->number_of_entries,
                    channel->segments[start_segment].name);
            fprintf(stdout, "%s", message);
            if (logfile) fprintf(lfp, "%s", message);
        }
        
        
        // verify index integrity within segments
        for (i=1; i<number_of_blocks; i++)
        {
            offset = (si8)(channel->segments[start_segment].time_series_indices_fps->time_series_indices[i].file_offset -
                           channel->segments[start_segment].time_series_indices_fps->time_series_indices[i-1].file_offset);
            if (offset > channel->segments[start_segment].metadata_fps->metadata.time_series_section_2->maximum_block_bytes || offset < 0)
            {
                num_errors++; bad_index = 1;
                sprintf(message, "Bad block index offset %ld between block %d and %d in segment %s, max_block_bytes = %d\n",
                        offset, i-1, i, channel->segments[start_segment].name, channel->segments[start_segment].metadata_fps->metadata.time_series_section_2->maximum_block_bytes);
                fprintf(stdout, "%s", message);
                if (logfile) fprintf(lfp, "%s", message);
                
            }
            
            dt = (si8)(channel->segments[start_segment].time_series_indices_fps->time_series_indices[i].start_time -
                       channel->segments[start_segment].time_series_indices_fps->time_series_indices[i-1].start_time);
            if (dt < 0) {
                num_errors++; bad_index = 1;
                sprintf(message, "Bad block timestamps: %lu in block %d and %lu in block %d (diff %ld) in segment %s\n",
                        channel->segments[start_segment].time_series_indices_fps->time_series_indices[i-1].start_time,
                        i-1,
                        channel->segments[start_segment].time_series_indices_fps->time_series_indices[i].start_time,
                        i,
                        dt,
                        channel->segments[start_segment].name);
                fprintf(stdout, "%s", message);
                if (logfile) fprintf(lfp, "%s", message);
            }
            
            ds = (si8)(channel->segments[start_segment].time_series_indices_fps->time_series_indices[i].start_sample -
                       channel->segments[start_segment].time_series_indices_fps->time_series_indices[i-1].start_sample);
            if (ds > channel->segments[start_segment].metadata_fps->metadata.time_series_section_2->maximum_block_samples || ds < 0) {
                num_errors++; bad_index = 1;
                sprintf(message, "Bad block sample numbers: %lu in block %d and %lu in block %d in segment %s\n",
                        channel->segments[start_segment].time_series_indices_fps->time_series_indices[i-1].start_sample,
                        i-1,
                        channel->segments[start_segment].time_series_indices_fps->time_series_indices[i].start_sample,
                        i,
                        channel->segments[start_segment].name);
                fprintf(stdout, "%s", message);
                if (logfile) fprintf(lfp, "%s", message);
            }
        }
        
        //fprintf(stderr, " number of blocks = %d\n", channel->segments[start_segment].time_series_indices_fps->universal_header->number_of_entries);
        
        if ((channel->segments[start_segment].time_series_indices_fps->time_series_indices[channel->segments[start_segment].time_series_indices_fps->universal_header->number_of_entries-1].start_sample +
             channel->segments[start_segment].time_series_indices_fps->time_series_indices[channel->segments[start_segment].time_series_indices_fps->universal_header->number_of_entries-1].number_of_samples) !=
            channel->segments[start_segment].metadata_fps->metadata.time_series_section_2->number_of_samples)
        {
            num_errors++;
            sprintf(message, "Number of samples in metadata for segment %s is %d, but total samples in index array is %d\n",
                    channel->segments[start_segment].name, channel->segments[start_segment].metadata_fps->metadata.time_series_section_2->number_of_samples,
                    (channel->segments[start_segment].time_series_indices_fps->time_series_indices[channel->segments[start_segment].time_series_indices_fps->universal_header->number_of_entries-1].start_sample +
                     channel->segments[start_segment].time_series_indices_fps->time_series_indices[channel->segments[start_segment].time_series_indices_fps->universal_header->number_of_entries-1].number_of_samples));
            fprintf(stdout, "%s", message);
            if (logfile) fprintf(lfp, "%s", message);
        }
        
        start_segment++;
        
    }
    
    // create RED processing struct
    rps = (RED_PROCESSING_STRUCT *) calloc((size_t) 1, sizeof(RED_PROCESSING_STRUCT));
    
    start_segment = 0;
    
    fprintf(stdout, "\n");
    
    // ************************
    // Iterate over segments, looping through data blocks
    // ************************
    
    while (start_segment < numSegments)
    {
        
        fprintf(stdout, "- Examining data of segment %s\n", channel->segments[start_segment].name);
        
        errors_before_this_segment = num_errors;
        
        data_end = channel->segments[start_segment].time_series_indices_fps->time_series_indices[0].file_offset;
        
        number_of_blocks = channel->segments[start_segment].time_series_indices_fps->universal_header->number_of_entries;
        
        
        //Loop through data blocks
        for (i=0; i<number_of_blocks; i++) {
            
            if(channel->segments[start_segment].time_series_indices_fps->time_series_indices[i].file_offset == data_end) { //read data
                if (i + blocks_per_read >= number_of_blocks) {
                    blocks_per_read = number_of_blocks - i;
                    data_end = channel->segments[start_segment].time_series_data_fps->file_length;
                }
                else {
                    data_end = channel->segments[start_segment].time_series_indices_fps->time_series_indices[blocks_per_read + i].file_offset;
                }
                
                
                data_start = channel->segments[start_segment].time_series_indices_fps->time_series_indices[i].file_offset;
                fseek(fp = channel->segments[start_segment].time_series_data_fps->fp,
                      channel->segments[start_segment].time_series_indices_fps->time_series_indices[i].file_offset, SEEK_SET);
                //fprintf(stdout, "data_end = %d\n", data_end);
                n = fread(data, 1, data_end - channel->segments[start_segment].time_series_indices_fps->time_series_indices[i].file_offset,
                          channel->segments[start_segment].time_series_data_fps->fp);
                if (n != data_end - channel->segments[start_segment].time_series_indices_fps->time_series_indices[i].file_offset ||
                    ferror(channel->segments[start_segment].time_series_data_fps->fp)) {
                    fprintf(stdout, "n = %ld, data_end = %ld, offset = %ld\n", n, data_end, channel->segments[start_segment].time_series_indices_fps->time_series_indices[i].file_offset);
                    fprintf(stdout, "[%s] Error reading mef data %s\n", __FUNCTION__, channelname);
                    fclose(lfp);
                    return(1);
                }
            }
            
            offset = channel->segments[start_segment].time_series_indices_fps->time_series_indices[i].file_offset - data_start;
            
            
            // cast block header
            rps->block_header = (RED_BLOCK_HEADER *) ((si1*)data + offset);
            
            //check that the block length agrees with index array to within 8 bytes
            //(differences less than 8 bytes caused by padding to maintain boundary alignment)
            if (i < number_of_blocks-1)
                block_size = channel->segments[start_segment].time_series_indices_fps->time_series_indices[i+1].file_offset -
                channel->segments[start_segment].time_series_indices_fps->time_series_indices[i].file_offset;
            else
                block_size = channel->segments[start_segment].time_series_data_fps->file_length -
                channel->segments[start_segment].time_series_indices_fps->time_series_indices[i].file_offset;
            
            
            // MEF 3: block_byte field in header now includes header and pad sizes
            if ( abs(block_size - rps->block_header->block_bytes) > 0 )
            {
                num_errors++;
                sprintf(message, "Block %d size %u disagrees with index array offset %u\, in segment %s\n", i,
                        rps->block_header->block_bytes, block_size, channel->segments[start_segment].name);
                fprintf(stdout, "%s", message);
                if (logfile) fprintf(lfp, "%s", message);
            }
            else //DON'T check CRC if block size is wrong- will crash the program
            {
                crc = CRC_calculate((ui1 *) rps->block_header + CRC_BYTES, rps->block_header->block_bytes - CRC_BYTES);
                
                if (crc != rps->block_header->block_CRC) {
                    num_errors++;
                    sprintf(message, "**CRC error in block %d in segment %s\n", i, channel->segments[start_segment].name);
                    fprintf(stdout, "%s", message);
                    //fprintf(stdout, "samples %d time %lu diff_count %d max %d min %d discontinuity %d\n", bk_hdr.sample_count,
                    //        rps->block_header->start_time, bk_hdr.difference_count, bk_hdr.max_value, bk_hdr.min_value,
                    //        bk_hdr.discontinuity);
                    if (logfile) {
                        fprintf(lfp, "%s", message);
                        //fprintf(lfp, "samples %d time %lu diff_count %d max %d min %d discontinuity %d\n", bk_hdr.sample_count,
                        //       bk_hdr.block_start_time, bk_hdr.difference_count, bk_hdr.max_value, bk_hdr.min_value,
                        //        bk_hdr.discontinuity);
                    }
                }
            }
            
            
            temp_time = rps->block_header->start_time;
            remove_recording_time_offset(&temp_time);
            
            // check that RED block start_time matches index entry start_time
            if (channel->segments[start_segment].time_series_indices_fps->time_series_indices[i].start_time != temp_time)
            {
                num_errors++;
                sprintf(message, "Block %d start_time does not match index start_time in segment %s\n", i, channel->segments[start_segment].name);
                fprintf(stdout, "%s", message);
                if (logfile) fprintf(lfp, "%s", message);
            }
            
            //check data block boundary alignment in file
            if (channel->segments[start_segment].time_series_indices_fps->time_series_indices[i].file_offset % 8) {
                num_errors++;
                sprintf(message, "Block %d is not 8-byte boundary aligned in segment %s\n,", i, channel->segments[start_segment].name);
                fprintf(stdout, "%s", message);
                if (logfile) fprintf(lfp, "%s", message);
            }
            
            temp_time2 = channel->segments[start_segment].metadata_fps->universal_header->start_time;
            remove_recording_time_offset(&temp_time2);
            
            if (temp_time < temp_time2) {
                num_errors++;
                sprintf(message, "%Block %d start time %lu is earlier than segment start time in segment %s\n",
                        i, temp_time, channel->segments[start_segment].name);
                fprintf(stdout, "%s", message);
                if (logfile) fprintf(lfp, "%s", message);
            }
            
            temp_time2 = channel->segments[start_segment].metadata_fps->universal_header->end_time;
            remove_recording_time_offset(&temp_time2);
            
            if (temp_time > temp_time2) {
                num_errors++;
                sprintf(message, "Block %d start time %lu is later than segment end time in segment %s\n",
                        i, temp_time, channel->segments[start_segment].name);
                fprintf(stdout, "%s", message);
                if (logfile) fprintf(lfp, "%s", message);
            }
        }
        
        now = time(NULL);
        sprintf(message, "%s check of %lu data blocks completed in segment %s with %lu errors found.\n\n", channelname,
                number_of_blocks, channel->segments[start_segment].name, num_errors - errors_before_this_segment);
        //fprintf(stdout, "%s", message);
        if (logfile) fprintf(lfp, "%s", message);
        
        start_segment++;
    }
    
    
    sprintf(message, "\nDone checking channel %s, total errors found is %d.\n\n", channelname, num_errors);
    fprintf(stdout, "%s", message);
    if (logfile) fprintf(lfp, "%s", message);
    
    // TBD free memory
    free(data); data = NULL;
    
    if (logfile) fclose(lfp);
    
    return(num_errors);
    
}

int main (int argc, const char * argv[]) {
    si4 i,j,k,m, numBlocks, start_block;
    si4 *data;
    ui8 inDataLength, outDataLength;
    ui1 *in_data;
    si4 numSegments, start_segment;
    si8 temp_time;
    si1 output_dir[1024];
    si1 *password;
    
    CHANNEL    *channel;
    RED_PROCESSING_STRUCT	*rps;
    ui4			max_samps;
    FILE *fp;
    si4 n_read;
    
    (void) initialize_meflib();
    
    password = NULL;
    
    if (argc < 2)
    {
        (void) printf("USAGE: %s chan_folder[s] [-p password]\n", argv[0]);
        return(1);
    }
    
    // look for password
    i = 1;
    while (i < argc)
    {
        if (*argv[i] == '-') {
            switch (argv[i][1])
            {
                case 'p':
                    if (i >= argc)
                    {
                        (void) printf("USAGE: %s chan_folder[s] [-p password]\n", argv[0]);
                        return(1);
                    }
                    password = argv[i+1];
                    i++;
                    break;
            }
        }
        i++;
    }
    
    if (argc > 1000)
    {
        printf("Too many folders specified!\n");
        return(1);
    }
    
    // iterate through files and validate.
    i = 1;
    while (i < argc)
    {
        if (*argv[i] == '-') {
            switch (argv[i][1])
            {
                // skip password flag and password
                case 'p':
                    i++;
                    i++;
                    continue;
                    break;
            }
        }

        validate_mef3(argv[i], "test.log", password);
        i++;
    }
    
    printf("Done.\n");
    
    return (0);
}
