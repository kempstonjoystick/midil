
/* Overview
 midil maps midi inputs (which alsa MIDI devices we can write to) with midi 
 outputs (things we read from).
 We allow the user to configure and maintain a relationship of midi inputs to outputs.
 We only need to maintain the list at one end (the output - the thing we read from)
 So each output has a list of inputs it is mapped to.

C program:
(1) populate a list of midi ports:clients - done
(2) Put this in a JSON file (for use with the webapp or bluetooth app) - done
(3) use a "mapping" file (populated by bluetooth or webapp) that manages the many 
    to one relationship between I/O
(4) Detect changes to the mapping, and use alsa to bind the ports
(5) Add a TTY midi interface to our DIN MIDI connectors
(6) enhance the above so the USB physical port is loosely mapped to each device.
The reason is that the port number might change depending on what is plugged in and when.
We really want to map devices by name
*/

/*
Copyright (c) 2017 Tim Shearer

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <alsa/asoundlib.h>
#include <json-c/json.h>

#define MIDIL_DIR "/opt/midil"
#define CURRENT_JSON_FILE "/tmp/midil_current.json"
#define MAPPING_JSON_FILE "/opt/midil/midil_mappings.json"

//TODO get rid of globals
static snd_seq_t* seq_handle;


/************************************
* intitialize ourselves, get our client ID
************************************/
static int init_seq(void)
{
    int err;
    int client;

    /* open the sequencer */
    if (snd_seq_open(&seq_handle, "hw", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
        fprintf(stderr, "Error opening ALSA sequencer.\n");
        exit(1);
    }

    /* set our name (otherwise it's "Client-xxx") */
    if (snd_seq_set_client_name(seq_handle, "midil") < 0) {
        fprintf(stderr, "Error setting client name\n");
        exit(1);
    }

    /* find out who we actually are */
    client = snd_seq_client_id(seq_handle);
    if (client < 0) {
    fprintf(stderr, "Failed to get client ID\n");
        exit(1);
    }
    return client;
}

/************************************
* get_clients_and_ports - populate the jconfig object
* and write the data to a file
*
*************************************/
static void get_clients_and_ports(json_object *jconfig)
{
    unsigned int port_capability;
    int port_rw_capable;

    snd_seq_client_info_t *client_info;
    snd_seq_port_info_t *port_info;

    //this will be a list of clients
    json_object *jclients = json_object_new_array();
   
    snd_seq_client_info_alloca(&client_info);
    snd_seq_port_info_alloca(&port_info);

    snd_seq_client_info_set_client(client_info, -1);
    while (snd_seq_query_next_client(seq_handle, client_info) >= 0) {
        int client = snd_seq_client_info_get_client(client_info);
        // here's the client object
        json_object *jclient = json_object_new_object();

        snd_seq_port_info_set_client(port_info, client);
        snd_seq_port_info_set_port(port_info, -1);

        // json name string
        json_object *jnamestring = json_object_new_string(snd_seq_client_info_get_name(client_info));
        json_object_object_add(jclient,"name", jnamestring);

        //json client index
        json_object *jclientindex = json_object_new_int(snd_seq_port_info_get_client(port_info));
        json_object_object_add(jclient,"index", jclientindex);

        json_object *jinports = json_object_new_array();
        json_object *joutports = json_object_new_array();

        //only interested in clients with read or writable ports
        port_rw_capable = 0;
        //get the writable (input from the perspective of the midi device) ports
        while (snd_seq_query_next_port(seq_handle, port_info) >= 0) {

            // port must understand MIDI messages 
            if (!(snd_seq_port_info_get_type(port_info)
                  & SND_SEQ_PORT_TYPE_MIDI_GENERIC))
                continue;

            port_capability = snd_seq_port_info_get_capability(port_info);
            //for inputs (things we write to)
            if ( port_capability & (SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE)
                == (SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE)) {

                json_object *jinport = json_object_new_object();

                json_object *jinportnamestring = json_object_new_string(snd_seq_port_info_get_name(port_info));
                json_object_object_add(jinport,"name", jinportnamestring);   

                json_object_array_add(jinports, jinport);            

                port_rw_capable += 1;
            }
            if ( port_capability & (SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ)
                == (SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ)) {

                json_object *joutport = json_object_new_object();

                json_object *joutportnamestring = json_object_new_string(snd_seq_port_info_get_name(port_info));
                json_object_object_add(joutport,"name", joutportnamestring);   

                json_object_array_add(joutports, joutport);        

                port_rw_capable += 1;    
            }
        }

        //add the port lists to the client
        json_object_object_add(jclient,"input_ports", jinports);
        json_object_object_add(jclient,"output_ports", joutports);

        //add the client to our clients array if it contains read/write ports
        if(port_rw_capable)
            json_object_array_add(jclients, jclient);

    }

    // add our list of clients to the master object
    json_object_object_add(jconfig,"clients", jclients);

    // write data to a file
    FILE *fp = fopen(CURRENT_JSON_FILE, "wb+");
    if (fp != NULL) {
        fprintf(fp, "%s", json_object_to_json_string_ext(jconfig, JSON_C_TO_STRING_PRETTY));
        fclose(fp);

        printf("Current MIDI configuration is now stored in: %s\n", CURRENT_JSON_FILE);
    }
    else {
        printf("Error: %s\n", strerror(errno));
    }
}

/* mapping data will look something like this:

mappings: [
{
  client: 1,
  name: nanoKEY2  
  mapped_to: [
      {
        client: 2,
        name: minilogue,
        input_port: 2
      },
      etc...
  ]
},
etc...
]
*/
static void get_mapping_data(json_object *jmapping) {

    char* buffer;
    long fSize;

    json_tokener * jtok = json_tokener_new();
    
    // read mapping data from a file
    FILE *fp = fopen(MAPPING_JSON_FILE, "r");
    if(fp != NULL) {
        fseek( fp , 0L , SEEK_END);
        fSize = ftell( fp );
        rewind( fp );

        /* allocate memory plus null */
        buffer = calloc( 1, fSize+1 );

        if( !buffer ) {
            printf("calloc: %s\n", strerror(errno));       
        } else {
            /* read in the data */
            if( fread( buffer , fSize, 1 , fp) != 1 )
                printf("Read failed: %s\n", strerror(errno));
            else {

                printf("%s\n", buffer);

                jmapping = json_tokener_parse_ex(jtok, buffer, strlen(buffer));
                if(!jmapping) {
                    printf("Failed to import mapping data\n");
                }                
            }

            free(buffer);
        }

        fclose(fp);
    }   
    printf("No MIDI mapping file found\n");

}

int main(int argc, char *argv[]) {

    struct stat st = {0};

    if (stat(MIDIL_DIR, &st) == -1) {
        printf("creating MIDIL dir\n");
        mkdir(MIDIL_DIR, 0700);
    }

    //initialize alsa sequencer
    int client = init_seq();

    json_object * jconfig = json_object_new_object();

    //get port structure
    get_clients_and_ports(jconfig);

    //find the mapping data - should be stored in another file
    json_object * jmapping = json_object_new_object();
    get_mapping_data(jmapping);

    //apply the mapping data to the object, where possible.

    //write the json file to disk

    //apply the aconnect rules as per the mapping

    //while loop
    // periodically check to see if there are new clients.
    // If so, update the json object
    // re-apply the mapping, basically attempting to reconcile any missing ones
    // check to see if there's a new json mapping file from webapp or bluetooth c
    // if so, apply those changes, update the mapping file


    //cleanup - check the json-c documentation
    if(jconfig)
        json_object_put(jconfig);
    if(jmapping)
        json_object_put(jmapping);
}