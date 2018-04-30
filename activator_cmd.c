#include "activator_cmd.h"
#include <unistd.h>
#include "errors.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int get_cmd_type(char key)
{
    if(key>='0' && key<='9')
    {
        return CMD_TYPE_ACTIVATE;
    }
    else if(key=='P')
    {
        return CMD_TYPE_PING;
    }
    else if(key=='S')
    {
        return CMD_TYPE_SUBSCRIBE;
    }
    else if(key=='T')
    {
        return CMD_TYPE_TRANSFER;
    }
    else if(key=='G')
    {
        return CMD_TYPE_GETSTATUS;
    }
    return CMD_TYPE_UNKNOWN;
}

int parse_cmd(activator_cmd * cmd, char * buf, size_t buf_len)
{
    if(cmd==NULL)
    {
        return DIAE_PARAM_IS_NULL;
    }
    if(buf==NULL)
    {
        return DIAE_PARAM_IS_NULL;
    }
    memset(cmd, 0 , sizeof(activator_cmd));


    //Let's parse

    //1st command type
    cmd->cmd_type=get_cmd_type(buf[0]);
    if(cmd->cmd_type<0)
    {
        printf("can't parse command(wrong type)_:\n%s\n",buf);
        return DIAE_COMMAND_WRONG_TYPE;
    }

    int i=0;
    if(cmd->cmd_type!=CMD_TYPE_ACTIVATE)
    {
        i=1; //all commands except for CMD_TYPE_ACTIVATE skip first symbol
    }

    //PARSE CHANNEL NUMBER
    while(buf[i]>='0' && buf[i]<='9')
    {
        cmd->channel*=10;
        cmd->channel+=buf[i];
        cmd->channel-='0';
        i++;
        if(i>=buf_len)
        {
            printf("can't parse command (ends on channel)_:\n%s\n",buf);
            return DIAE_COMMAND_UNEXPECTED_END;
        }
    }
    if(cmd->channel<0)
    {
        cmd->channel = 0;
        printf("can't parse command (channel)_:\n%s\n",buf);
        return DIAE_COMMAND_WRONG_FORMAT;
    }
    else if (cmd->channel > MAX_CHANNELS_SUPPORTED)
    {
        cmd->channel = MAX_CHANNELS_SUPPORTED-1;
        printf("can't parse command (channel)_:\n%s\n",buf);
        return DIAE_COMMAND_WRONG_FORMAT;
    }

     //should be ':'
    if(buf[i]!=':')
    {
        printf("can't parse command (no ':')_:\n%s\n",buf);
        return DIAE_COMMAND_WRONG_FORMAT;
    }

    ///////////////////////////////////////////////////////
    int cmd_bytes_written = 0;
    for(int j=i+1;(j<(buf_len-1))&&(cmd_bytes_written<(MAX_CMD-1));j++)
    {
        cmd->data[cmd_bytes_written] = buf[j];
        if(buf[j]==';')
        {
            cmd->data[cmd_bytes_written+1]=0;
            return DIAE_OK;
        }
        if(buf[j]==0)
        {
            printf("zero terminated command (instead of ;)\n");
            return DIAE_COMMAND_NOT_TERMINATED;
        }
        cmd_bytes_written++;
    }
    ///////////////////////////////////////////////////////



    printf("can't parse command_:\n%s\n",buf);
    return DIAE_COMMAND_EMPTY;

}
