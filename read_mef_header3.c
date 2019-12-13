/*

 *  read_mef_header3.c
 * 
 
 Program to read mef format file (v3.0) basic header and metadata information to standard out.
 
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
    FILE_PROCESSING_STRUCT *temp_fps;
    
    
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
    
    temp_fps = allocate_file_processing_struct(0, TIME_SERIES_METADATA_FILE_TYPE_CODE, NULL, NULL, 0);
    temp_fps->metadata = channel->metadata;
    temp_fps->password_data = channel->segments[0].metadata_fps->password_data;
    
    show_universal_header(channel->segments[0].metadata_fps);
    show_metadata(temp_fps);
    
    return 0;
}
