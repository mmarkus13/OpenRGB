/*-----------------------------------------*\
|  NetworkClient.cpp                        |
|                                           |
|  Client code for OpenRGB SDK              |
|                                           |
|  Adam Honse (CalcProgrammer1) 5/9/2020    |
\*-----------------------------------------*/

#include "NetworkClient.h"
#include "RGBController_Network.h"
#include <cstring>

#ifdef _WIN32
#include <Windows.h>
#define MSG_NOSIGNAL 0
#endif

#ifdef __APPLE__
#include <unistd.h>
#define MSG_NOSIGNAL 0

static void Sleep(unsigned int milliseconds)
{
    usleep(1000 * milliseconds);
}
#endif

#ifdef __linux__
#include <unistd.h>

static void Sleep(unsigned int milliseconds)
{
    usleep(1000 * milliseconds);
}
#endif

NetworkClient::NetworkClient(std::vector<RGBController *>& control) : controllers(control)
{
    strcpy(port_ip, "127.0.0.1");
    port_num                = 1337;
    server_connected        = false;
    server_controller_count = 0;
}

void NetworkClient::ClientInfoChanged()
{
    ClientInfoChangeMutex.lock();

    /*-------------------------------------------------*\
    | Client info has changed, call the callbacks       |
    \*-------------------------------------------------*/
    for(unsigned int callback_idx = 0; callback_idx < ClientInfoChangeCallbacks.size(); callback_idx++)
    {
        ClientInfoChangeCallbacks[callback_idx](ClientInfoChangeCallbackArgs[callback_idx]);
    }

    ClientInfoChangeMutex.unlock();
}

const char * NetworkClient::GetIP()
{
    return(port_ip);
}

unsigned short NetworkClient::GetPort()
{
    return port_num;
}

bool NetworkClient::GetOnline()
{
    return(server_connected && server_initialized);
}

void NetworkClient::RegisterClientInfoChangeCallback(NetClientCallback new_callback, void * new_callback_arg)
{
    ClientInfoChangeCallbacks.push_back(new_callback);
    ClientInfoChangeCallbackArgs.push_back(new_callback_arg);
}

void NetworkClient::SetIP(const char *new_ip)
{
    if(server_connected == false)
    {
        strcpy(port_ip, new_ip);
    }
}

void NetworkClient::SetName(const char *new_name)
{
    client_name = new_name;

    if(server_connected == true)
    {
        SendData_ClientString();
    }
}

void NetworkClient::SetPort(unsigned short new_port)
{
    if(server_connected == false)
    {
        port_num = new_port;
    }
}

void NetworkClient::StartClient()
{
    //Start a TCP server and launch threads
    char port_str[6];
    snprintf(port_str, 6, "%d", port_num);

    port.tcp_client(port_ip, port_str);

    client_active = true;

    //Start the connection thread
    ConnectionThread = new std::thread(&NetworkClient::ConnectionThreadFunction, this);

    /*-------------------------------------------------*\
    | Client info has changed, call the callbacks       |
    \*-------------------------------------------------*/
    ClientInfoChanged();
}

void NetworkClient::StopClient()
{
    server_connected = false;
    client_active    = false;

    shutdown(client_sock, SD_RECEIVE);
    closesocket(client_sock);
    ListenThread->join();
    ConnectionThread->join();

    /*-------------------------------------------------*\
    | Client info has changed, call the callbacks       |
    \*-------------------------------------------------*/
    ClientInfoChanged();
}

void NetworkClient::ConnectionThreadFunction()
{
    unsigned int requested_controllers;

    //This thread manages the connection to the server
    while(client_active == true)
    {
        if(server_connected == false)
        {
            //Connect to server and reconnect if the connection is lost
            server_initialized = false;

            //Try to connect to server
            if(port.tcp_client_connect() == true)
            {
                client_sock = port.sock;
                printf( "Connected to server\n" );

                //Server is now connected
                server_connected = true;

                //Start the listener thread
                ListenThread = new std::thread(&NetworkClient::ListenThreadFunction, this);

                //Server is not initialized
                server_initialized = false;

                /*-------------------------------------------------*\
                | Client info has changed, call the callbacks       |
                \*-------------------------------------------------*/
                ClientInfoChanged();
            }
            else
            {
                printf( "Connection attempt failed\n" );
            }
        }

        if(server_initialized == false && server_connected == true)
        {
            requested_controllers   = 0;
            server_controller_count = 0;

            //Wait for server to connect
            Sleep(100);

            //Once server is connected, send client string
            SendData_ClientString();

            //Request number of controllers
            SendRequest_ControllerCount();

            //Wait for server controller count
            while(server_controller_count == 0)
            {
                Sleep(100);
            }

            printf("Client: Received controller count from server: %d\r\n", server_controller_count);

            //Once count is received, request controllers
            while(requested_controllers < server_controller_count)
            {
                printf("Client: Requesting controller %d\r\n", requested_controllers);

                SendRequest_ControllerData(requested_controllers);

                //Wait until controller is received
                while(server_controllers.size() == requested_controllers)
                {
                    Sleep(100);
                }

                requested_controllers++;
            }

            //All controllers received, add them to master list
            printf("Client: All controllers received, adding them to master list\r\n");
            for(std::size_t controller_idx = 0; controller_idx < server_controllers.size(); controller_idx++)
            {
                controllers.push_back(server_controllers[controller_idx]);
            }

            server_initialized = true;

            /*-------------------------------------------------*\
            | Client info has changed, call the callbacks       |
            \*-------------------------------------------------*/
            ClientInfoChanged();
        }

        Sleep(1000);
    }
}

int NetworkClient::recv_select(SOCKET s, char *buf, int len, int flags)
{
    fd_set              set;
    struct timeval      timeout;
    timeout.tv_sec      = 5;
    timeout.tv_usec     = 0;

    while(1)
    {
        FD_ZERO(&set);      /* clear the set */
        FD_SET(s, &set);    /* add our file descriptor to the set */

        int rv = select(s + 1, &set, NULL, NULL, &timeout);

        if(rv == SOCKET_ERROR || server_connected == false)
        {
            return 0;
        }
        else if(rv == 0)
        {
            continue;
        }
        else
        {
            // socket has something to read
            return(recv(s, buf, len, flags));
        }
    }
}

void NetworkClient::ListenThreadFunction()
{
    printf("Network client listener started\n");
    //This thread handles messages received from the server
    while(server_connected == true)
    {
        NetPacketHeader header;
        int             bytes_read  = 0;
        char *          data        = NULL;

        //Read first byte of magic
        bytes_read = recv_select(client_sock, &header.pkt_magic[0], 1, 0);

        if(bytes_read <= 0)
        {
            goto listen_done;
        }

        //Test first character of magic - 'O'
        if(header.pkt_magic[0] != 'O')
        {
            continue;
        }

        //Read second byte of magic
        bytes_read = recv_select(client_sock, &header.pkt_magic[1], 1, 0);

        if(bytes_read <= 0)
        {
            goto listen_done;
        }

        //Test second character of magic - 'R'
        if(header.pkt_magic[1] != 'R')
        {
            continue;
        }

        //Read third byte of magic
        bytes_read = recv_select(client_sock, &header.pkt_magic[2], 1, 0);

        if(bytes_read <= 0)
        {
            goto listen_done;
        }

        //Test third character of magic - 'G'
        if(header.pkt_magic[2] != 'G')
        {
            continue;
        }

        //Read fourth byte of magic
        bytes_read = recv_select(client_sock, &header.pkt_magic[3], 1, 0);

        if(bytes_read <= 0)
        {
            goto listen_done;
        }

        //Test fourth character of magic - 'B'
        if(header.pkt_magic[3] != 'B')
        {
            continue;
        }

        //If we get to this point, the magic is correct.  Read the rest of the header
        bytes_read = 0;
        do
        {
            int tmp_bytes_read = 0;

            tmp_bytes_read = recv_select(client_sock, (char *)&header.pkt_dev_idx + bytes_read, sizeof(header) - sizeof(header.pkt_magic) - bytes_read, 0);

            bytes_read += tmp_bytes_read;

            if(tmp_bytes_read <= 0)
            {
                goto listen_done;
            }

        } while(bytes_read != sizeof(header) - sizeof(header.pkt_magic));

        //printf( "Client: Received header, now receiving data of size %d\r\n", header.pkt_size);

        //Header received, now receive the data
        if(header.pkt_size > 0)
        {
            bytes_read = 0;

            data = new char[header.pkt_size];

            do
            {
                int tmp_bytes_read = 0;

                tmp_bytes_read = recv_select(client_sock, &data[bytes_read], header.pkt_size - bytes_read, 0);

                if(tmp_bytes_read <= 0)
                {
                    goto listen_done;
                }
                bytes_read += tmp_bytes_read;

            } while (bytes_read < header.pkt_size);
        }

        //printf( "Client: Received header and data\r\n" );
        //printf( "Client: Packet ID: %d \r\n", header.pkt_id);

        //Entire request received, select functionality based on request ID
        switch(header.pkt_id)
        {
            case NET_PACKET_ID_REQUEST_CONTROLLER_COUNT:
                printf( "Client: NET_PACKET_ID_REQUEST_CONTROLLER_COUNT\r\n" );

                ProcessReply_ControllerCount(header.pkt_size, data);
                break;

            case NET_PACKET_ID_REQUEST_CONTROLLER_DATA:
                printf( "Client: NET_PACKET_ID_REQUEST_CONTROLLER_DATA\r\n");

                ProcessReply_ControllerData(header.pkt_size, data, header.pkt_dev_idx);
                break;
        }

        delete[] data;
    }

listen_done:
    printf( "Client socket has been closed");
    server_initialized = false;
    server_connected = false;

    for(int server_controller_idx = 0; server_controller_idx < server_controllers.size(); server_controller_idx++)
    {
        for(int controller_idx = 0; controller_idx < controllers.size(); controller_idx++)
        {
            if(controllers[controller_idx] == server_controllers[server_controller_idx])
            {
                controllers.erase(controllers.begin() + controller_idx);
                break;
            }
        }

        delete server_controllers[server_controller_idx];
    }

    server_controllers.clear();

    /*-------------------------------------------------*\
    | Client info has changed, call the callbacks       |
    \*-------------------------------------------------*/
    ClientInfoChanged();
}

void NetworkClient::ProcessReply_ControllerCount(unsigned int data_size, char * data)
{
    if(data_size == sizeof(unsigned int))
    {
        memcpy(&server_controller_count, data, sizeof(unsigned int));
    }
}

void NetworkClient::ProcessReply_ControllerData(unsigned int data_size, char * data, unsigned int dev_idx)
{
    RGBController_Network * new_controller = new RGBController_Network(this, dev_idx);

    new_controller->ReadDeviceDescription((unsigned char *)data);

    printf("Received controller: %s\r\n", new_controller->name.c_str());

    server_controllers.push_back(new_controller);
}

void NetworkClient::SendData_ClientString()
{
    NetPacketHeader reply_hdr;

    reply_hdr.pkt_magic[0] = 'O';
    reply_hdr.pkt_magic[1] = 'R';
    reply_hdr.pkt_magic[2] = 'G';
    reply_hdr.pkt_magic[3] = 'B';

    reply_hdr.pkt_dev_idx  = 0;
    reply_hdr.pkt_id       = NET_PACKET_ID_SET_CLIENT_NAME;
    reply_hdr.pkt_size     = strlen(client_name.c_str()) + 1;

    send(client_sock, (char *)&reply_hdr, sizeof(NetPacketHeader), MSG_NOSIGNAL);
    send(client_sock, (char *)client_name.c_str(), reply_hdr.pkt_size, MSG_NOSIGNAL);
}

void NetworkClient::SendRequest_ControllerCount()
{
    NetPacketHeader reply_hdr;

    reply_hdr.pkt_magic[0] = 'O';
    reply_hdr.pkt_magic[1] = 'R';
    reply_hdr.pkt_magic[2] = 'G';
    reply_hdr.pkt_magic[3] = 'B';

    reply_hdr.pkt_dev_idx  = 0;
    reply_hdr.pkt_id       = NET_PACKET_ID_REQUEST_CONTROLLER_COUNT;
    reply_hdr.pkt_size     = 0;

    send(client_sock, (char *)&reply_hdr, sizeof(NetPacketHeader), MSG_NOSIGNAL);
}

void NetworkClient::SendRequest_ControllerData(unsigned int dev_idx)
{
    NetPacketHeader reply_hdr;

    reply_hdr.pkt_magic[0] = 'O';
    reply_hdr.pkt_magic[1] = 'R';
    reply_hdr.pkt_magic[2] = 'G';
    reply_hdr.pkt_magic[3] = 'B';

    reply_hdr.pkt_dev_idx  = dev_idx;
    reply_hdr.pkt_id       = NET_PACKET_ID_REQUEST_CONTROLLER_DATA;
    reply_hdr.pkt_size     = 0;

    send(client_sock, (char *)&reply_hdr, sizeof(NetPacketHeader), MSG_NOSIGNAL);
}

void NetworkClient::SendRequest_RGBController_ResizeZone(unsigned int dev_idx, int zone, int new_size)
{
    NetPacketHeader reply_hdr;
    int             reply_data[2];

    reply_hdr.pkt_magic[0] = 'O';
    reply_hdr.pkt_magic[1] = 'R';
    reply_hdr.pkt_magic[2] = 'G';
    reply_hdr.pkt_magic[3] = 'B';

    reply_hdr.pkt_dev_idx  = dev_idx;
    reply_hdr.pkt_id       = NET_PACKET_ID_RGBCONTROLLER_RESIZEZONE;
    reply_hdr.pkt_size     = sizeof(reply_data);

    reply_data[0]          = zone;
    reply_data[1]          = new_size;

    send(client_sock, (char *)&reply_hdr, sizeof(NetPacketHeader), MSG_NOSIGNAL);
    send(client_sock, (char *)&reply_data, sizeof(reply_data), MSG_NOSIGNAL);
}

void NetworkClient::SendRequest_RGBController_UpdateLEDs(unsigned int dev_idx, unsigned char * data, unsigned int size)
{
    NetPacketHeader reply_hdr;

    reply_hdr.pkt_magic[0] = 'O';
    reply_hdr.pkt_magic[1] = 'R';
    reply_hdr.pkt_magic[2] = 'G';
    reply_hdr.pkt_magic[3] = 'B';

    reply_hdr.pkt_dev_idx  = dev_idx;
    reply_hdr.pkt_id       = NET_PACKET_ID_RGBCONTROLLER_UPDATELEDS;
    reply_hdr.pkt_size     = size;

    send(client_sock, (char *)&reply_hdr, sizeof(NetPacketHeader), MSG_NOSIGNAL);
    send(client_sock, (char *)data, size, 0);
}

void NetworkClient::SendRequest_RGBController_UpdateZoneLEDs(unsigned int dev_idx, unsigned char * data, unsigned int size)
{
    NetPacketHeader reply_hdr;

    reply_hdr.pkt_magic[0] = 'O';
    reply_hdr.pkt_magic[1] = 'R';
    reply_hdr.pkt_magic[2] = 'G';
    reply_hdr.pkt_magic[3] = 'B';

    reply_hdr.pkt_dev_idx  = dev_idx;
    reply_hdr.pkt_id       = NET_PACKET_ID_RGBCONTROLLER_UPDATEZONELEDS;
    reply_hdr.pkt_size     = size;

    send(client_sock, (char *)&reply_hdr, sizeof(NetPacketHeader), MSG_NOSIGNAL);
    send(client_sock, (char *)data, size, MSG_NOSIGNAL);
}

void NetworkClient::SendRequest_RGBController_UpdateSingleLED(unsigned int dev_idx, unsigned char * data, unsigned int size)
{
    NetPacketHeader reply_hdr;

    reply_hdr.pkt_magic[0] = 'O';
    reply_hdr.pkt_magic[1] = 'R';
    reply_hdr.pkt_magic[2] = 'G';
    reply_hdr.pkt_magic[3] = 'B';

    reply_hdr.pkt_dev_idx  = dev_idx;
    reply_hdr.pkt_id       = NET_PACKET_ID_RGBCONTROLLER_UPDATESINGLELED;
    reply_hdr.pkt_size     = size;

    send(client_sock, (char *)&reply_hdr, sizeof(NetPacketHeader), MSG_NOSIGNAL);
    send(client_sock, (char *)data, size, MSG_NOSIGNAL);
}

void NetworkClient::SendRequest_RGBController_SetCustomMode(unsigned int dev_idx)
{
    NetPacketHeader reply_hdr;

    reply_hdr.pkt_magic[0] = 'O';
    reply_hdr.pkt_magic[1] = 'R';
    reply_hdr.pkt_magic[2] = 'G';
    reply_hdr.pkt_magic[3] = 'B';

    reply_hdr.pkt_dev_idx  = dev_idx;
    reply_hdr.pkt_id       = NET_PACKET_ID_RGBCONTROLLER_SETCUSTOMMODE;
    reply_hdr.pkt_size     = 0;

    send(client_sock, (char *)&reply_hdr, sizeof(NetPacketHeader), MSG_NOSIGNAL);
}

void NetworkClient::SendRequest_RGBController_UpdateMode(unsigned int dev_idx, unsigned char * data, unsigned int size)
{
    NetPacketHeader reply_hdr;

    reply_hdr.pkt_magic[0] = 'O';
    reply_hdr.pkt_magic[1] = 'R';
    reply_hdr.pkt_magic[2] = 'G';
    reply_hdr.pkt_magic[3] = 'B';

    reply_hdr.pkt_dev_idx  = dev_idx;
    reply_hdr.pkt_id       = NET_PACKET_ID_RGBCONTROLLER_UPDATEMODE;
    reply_hdr.pkt_size     = size;

    send(client_sock, (char *)&reply_hdr, sizeof(NetPacketHeader), MSG_NOSIGNAL);
    send(client_sock, (char *)data, size, MSG_NOSIGNAL);
}
