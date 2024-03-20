#include <map>
// IOT socket api
#include <iot/socket.hpp>

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <chat.hpp>

#define USER_ALL "__ALL"
#define USER_END "END"

/**
 * @brief map of current online clients
*/
typedef std::map<std::string, sockaddr_in *> online_users;

void handle_list(
    online_users& online_users, std::string username, std::string,
    struct sockaddr_in& client_address, uwe::socket& sock, bool& exit_loop);

/**
 * @brief Send a given message to all clients
 *
 * @param msg to send
 * @param username used if  not to send to that particular user
 * @param online_users current online users
 * @param sock socket for communicting with client 
 * @param send_to_username determines also to send to username
*/
void send_all(
    chat::chat_message& msg, std::string username, online_users& online_users, 
    uwe::socket& sock, bool send_to_username = true) {
    for (const auto user: online_users) {    
        if ((send_to_username && user.first.compare(username) == 0) || user.first.compare(username) != 0) { 
            int len = sock.sendto(
                reinterpret_cast<const char*>(&msg), sizeof(chat::chat_message), 0,
                (sockaddr*)user.second, sizeof(struct sockaddr_in));
        }
    }   
}

/**
 * @brief handle sending an error and incoming error messages
 * 
 * Note: there should not be any incoming errors messages!
 * 
 * @param err code for error
 * @param client_address address of client to send message to
 * @param sock socket for communicting with client
 * @parm exit_loop set to true if event loop is to terminate
*/
void handle_error(uint16_t err, struct sockaddr_in& client_address, uwe::socket& sock, bool& exit_loop) {
    auto msg = chat::error_msg(err);
    int len = sock.sendto(
        reinterpret_cast<const char*>(&msg), sizeof(chat::chat_message), 0,
        (sockaddr*)&client_address, sizeof(struct sockaddr_in));
}

/**
 * @brief handle broadcast message
 * 
 * @param online_users map of usernames to their corresponding IP:PORT address
 * @param username part of chat protocol packet
 * @param msg part of chat protocol packet
 * @param client_address address of client to send message to
 * @param sock socket for communicting with client
 * @parm exit_loop set to true if event loop is to terminate
*/
void handle_broadcast(
    online_users& online_users, std::string username, std::string msg,
    struct sockaddr_in& client_address, uwe::socket& sock, bool& exit_loop) {
    
    DEBUG("Received broadcast\n");

    auto m = chat::broadcast_msg(username, msg); // Create the broadcast message once

    // Iterate over the map of online users and send the message to each user except the sender
    for (const auto& user_pair : online_users) {
        // Check if the user is not the sender
        if (client_address.sin_addr.s_addr != user_pair.second->sin_addr.s_addr ||
            client_address.sin_port != user_pair.second->sin_port) {

            // Send the broadcast message to the user
            int len = sock.sendto(
                reinterpret_cast<const char*>(&m), sizeof(chat::chat_message), 0,
                (struct sockaddr*)user_pair.second, sizeof(struct sockaddr_in));
                
            // Log the send operation
            DEBUG("Broadcast message sent to %s\n", user_pair.first.c_str());
        } else {
            // This is the sender, do not send the message back to them
            DEBUG("Not sending message to self: %s\n", msg.c_str());
        }
    }
}

/**
 * @brief handle join messageß
 * 
 * @param online_users map of usernames to their corresponding IP:PORT address
 * @param username part of chat protocol packet
 * @param msg part of chat protocol packet
 * @param client_address address of client to send message to
 * @param sock socket for communicting with client
 * @parm exit_loop set to true if event loop is to terminate
*/
void handle_join(
    online_users& users, std::string username, std::string,
    struct sockaddr_in& client_address, uwe::socket& sock, bool& exit_loop) {
    
    if (users.find(username) != users.end()) {
        handle_error(ERR_USER_ALREADY_ONLINE, client_address, sock, exit_loop);
    } else {
        auto* addr = new sockaddr_in(client_address);
        users[username] = addr;
        
        auto msg = chat::jack_msg();
        sock.sendto(reinterpret_cast<const char*>(&msg), sizeof(msg), 0, (sockaddr*)&client_address, sizeof(client_address));
        
        for (const auto& user : users) {
            if (user.first != username) {
                auto broadcastMsg = chat::broadcast_msg("Server", username + " has joined the chat.");
                sock.sendto(reinterpret_cast<const char*>(&broadcastMsg), sizeof(broadcastMsg), 0, (sockaddr*)user.second, sizeof(sockaddr_in));
            }
        }

        handle_list(users, USER_ALL, "", client_address, sock, exit_loop);
    }
}

/**
 * @brief handle jack message
 * 
 * @param online_users map of usernames to their corresponding IP:PORT address
 * @param username part of chat protocol packet
 * @param msg part of chat protocol packet
 * @param client_address address of client to send message to
 * @param sock socket for communicting with client
 * @parm exit_loop set to true if event loop is to terminate
*/
void handle_jack(
    online_users& online_users, std::string username, std::string, 
    struct sockaddr_in& client_address, uwe::socket& sock, bool& exit_loop) {
    DEBUG("Received jack\n");
    handle_error(ERR_UNEXPECTED_MSG, client_address, sock, exit_loop);
}

/**
 * @brief handle direct message
 * 
 * @param online_users map of usernames to their corresponding IP:PORT address
 * @param username part of chat protocol packet
 * @param msg part of chat protocol packet
 * @param client_address address of client to send message to
 * @param sock socket for communicting with client
 * @parm exit_loop set to true if event loop is to terminate
*/
// handle_directmessage implementation
void handle_directmessage(
    online_users& users, std::string username, std::string message,
    struct sockaddr_in& client_address, uwe::socket& sock, bool& exit_loop) {
    
    auto it = users.find(username);
    if (it != users.end()) {
        auto msg = chat::dm_msg(username, message);
        sock.sendto(reinterpret_cast<const char*>(&msg), sizeof(msg), 0, (sockaddr*)it->second, sizeof(sockaddr_in));
    } else {
        // handle_error(ERR_USER_NOT_FOUND, client_address, sock, exit_loop);
    }
}

/**
 * @brief handle list message
 * 
 * @param online_users map of usernames to their corresponding IP:PORT address
 * @param username part of chat protocol packet
 * @param msg part of chat protocol packet
 * @param client_address address of client to send message to
 * @param sock socket for communicting with client
 * @parm exit_loop set to true if event loop is to terminate
*/
void handle_list(
    online_users& online_users, std::string username, std::string,
    struct sockaddr_in& client_address, uwe::socket& sock, bool& exit_loop) {
    DEBUG("Received list\n");

    int username_size = MAX_USERNAME_LENGTH;
    int message_size  = MAX_MESSAGE_LENGTH;

    char username_data[MAX_USERNAME_LENGTH] = { '\0' };
    char * username_ptr = &username_data[0];
    char message_data[MAX_MESSAGE_LENGTH] = { '\0' };
    char * message_ptr = &message_data[0];

    bool using_username = true;
    bool full = false;

    for (const auto user: online_users) {
        if (using_username) {
            if (username_size - (user.first.length()+1) >= 0) {
                memcpy(username_ptr, user.first.c_str(), user.first.length());
                *(username_ptr+user.first.length()) = ':';
                username_ptr = username_ptr+user.first.length()+1;
                username_size = username_size - (user.first.length()+1);
                username_data[MAX_USERNAME_LENGTH - username_size] = '\0';
            }
            else {
                using_username = false;
            }
        }

        // otherwise we fill the message field
        if(!using_username) {
            if (message_size - (user.first.length()+1) >= 0) {
                memcpy(message_ptr, user.first.c_str(), user.first.length());
                *(message_ptr+user.first.length()) = ':';
                message_ptr = message_ptr+user.first.length()+1;
                message_size = message_size - (user.first.length()+1);
            }
            else {
                // we are full and we need to send packet and start again
                chat::chat_message msg{chat::LIST, '\0', '\0'};
                username_data[MAX_USERNAME_LENGTH - username_size] = '\0';
                memcpy(msg.username_, &username_data[0], MAX_USERNAME_LENGTH - username_size );
                message_data[MAX_MESSAGE_LENGTH - message_size] = '\0';
                memcpy(msg.message_, &message_data[0], MAX_MESSAGE_LENGTH - message_size );

                // 
                if (username.compare("__ALL") == 0) {
                    send_all(msg, "__ALL", online_users, sock);
                }
                else {
                    int len = sock.sendto(
                        reinterpret_cast<const char*>(&msg), sizeof(chat::chat_message), 0,
                        (sockaddr*)&client_address, sizeof(struct sockaddr_in));
                }

                username_size = MAX_USERNAME_LENGTH;
                message_size  = MAX_MESSAGE_LENGTH;

                username_ptr = &username_data[0];
                message_ptr = &message_data[0];

                using_username = false;
            }
        }
    }

    if (using_username) {
        if (username_size >= 4) { 
            // enough space to store end in username
            memcpy(&username_data[MAX_USERNAME_LENGTH - username_size], USER_END, strlen(USER_END) );
            username_size = username_size - (strlen(USER_END)+1);
        }
        else {
            username_size = username_size + 1; // this enables overwriting the last ':'
            using_username = false;
        }
    }
    
    if (!using_username) {

    }

    chat::chat_message msg{chat::LIST, '\0', '\0'};
    username_data[MAX_USERNAME_LENGTH - username_size] = '\0';
    DEBUG("username_data = %s\n", username_data);
    memcpy(msg.username_, &username_data[0], MAX_USERNAME_LENGTH - username_size );
    message_data[MAX_MESSAGE_LENGTH - message_size] = '\0';
    memcpy(msg.message_, &message_data[0], MAX_MESSAGE_LENGTH - message_size );

    if (username.compare("__ALL") == 0) {
        send_all(msg, "__ALL", online_users, sock);
    }
    else {
        int len = sock.sendto(
            reinterpret_cast<const char*>(&msg), sizeof(chat::chat_message), 0,
            (sockaddr*)&client_address, sizeof(struct sockaddr_in));
    }
}

/**
 * @brief handle leave message
 * 
 * @param online_users map of usernames to their corresponding IP:PORT address
 * @param username part of chat protocol packet
 * @param msg part of chat protocol packet
 * @param client_address address of client to send message to
 * @param sock socket for communicting with client
 * @parm exit_loop set to true if event loop is to terminate
*/
void handle_leave(
    online_users& online_users, std::string username, std::string,
    struct sockaddr_in& client_address, uwe::socket& sock, bool& exit_loop) {
    DEBUG("Received leave\n");

    username = "";
    // find username
    for (const auto user: online_users) {
        if (strcmp(inet_ntoa(client_address.sin_addr), inet_ntoa(user.second->sin_addr)) == 0 &&
            client_address.sin_port == user.second->sin_port) {
                username = user.first;
        }
    }
    DEBUG("%s is leaving the sever\n", username.c_str());

    if (username.length() == 0) {
        // this should never happen
        handle_error(ERR_UNKNOWN_USERNAME, client_address, sock, exit_loop); 
    }
    else if (auto search = online_users.find(username); search != online_users.end()) {
        // first free memory for sockaddr
        struct sockaddr_in * addr = search->second;
        delete addr;

        // now delete from username map
        online_users.erase(search);

        // finally send back LACK
        auto msg = chat::lack_msg();
        int len = sock.sendto(
            reinterpret_cast<const char*>(&msg), sizeof(chat::chat_message), 0,
            (sockaddr*)&client_address, sizeof(struct sockaddr_in));

        //handle_broadcast(online_users, username, "has left the server", client_address, sock, exit_loop);
        msg = chat::chat_message{chat::LEAVE, '\0', '\0'};
        memcpy(msg.username_, username.c_str(), username.length()+1);
        send_all(msg, username, online_users, sock, false);
    }
    else {
        handle_error(ERR_UNKNOWN_USERNAME, client_address, sock, exit_loop); 
    }
}

/**
 * @brief handle lack message
 * 
 * @param online_users map of usernames to their corresponding IP:PORT address
 * @param username part of chat protocol packet
 * @param msg part of chat protocol packet
 * @param client_address address of client to send message to
 * @param sock socket for communicting with client
 * @parm exit_loop set to true if event loop is to terminate
*/
void handle_lack(
    online_users& online_users, std::string username, std::string,
    struct sockaddr_in& client_address, uwe::socket& sock, bool& exit_loop) {
    DEBUG("Received lack\n");
    handle_error(ERR_UNEXPECTED_MSG, client_address, sock, exit_loop);
}

/**
 * @brief handle exit message
 * 
 * @param online_users map of usernames to their corresponding IP:PORT address
 * @param username part of chat protocol packet
 * @param msg part of chat protocol packet
 * @param client_address address of client to send message to
 * @param sock socket for communicting with client
 * @parm exit_loop set to true if event loop is to terminate
*/
void handle_exit(
    online_users& users, std::string, std::string, 
    struct sockaddr_in& client_address, uwe::socket& sock, bool& exit_loop) {
    
    for (const auto& user : users) {
        auto msg = chat::exit_msg();
        sock.sendto(reinterpret_cast<const char*>(&msg), sizeof(msg), 0, (sockaddr*)user.second, sizeof(sockaddr_in));
    }
    users.clear();
    exit_loop = true;
}

/**
 * @brief
 * 
 * @param online_users map of usernames to their corresponding IP:PORT address
 * @param username part of chat protocol packet
 * @param msg part of chat protocol packet
 * @param client_address address of client to send message to
 * @param sock socket for communicting with client
 * @parm exit_loop set to true if event loop is to terminate
*/
void handle_error(
    online_users& online_users, std::string username, std::string, 
    struct sockaddr_in& client_address, uwe::socket& sock, bool& exit_loop) {
     DEBUG("Received error\n");
}

/**
 * @brief function table, mapping command type to handler.
*/
void (*handle_messages[9])(online_users&, std::string, std::string, struct sockaddr_in&, uwe::socket&, bool& exit_loop) = {
    handle_join, handle_jack, handle_broadcast, handle_directmessage,
    handle_list, handle_leave, handle_lack, handle_exit, handle_error
};

/**
 * @brief server for chat protocol
*/
void server() {
    // keep track of online users
    online_users online_users;

    // port to start the server on

	// socket address used for the server
	struct sockaddr_in server_address;
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;

	// htons: host to network short: transforms a value in host byte
	// ordering format to a short value in network byte ordering format
	server_address.sin_port = htons(SERVER_PORT);

	// htons: host to network long: same as htons but to long
	// server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	// creates binary representation of server name and stores it as sin_addr
	inet_pton(AF_INET, uwe::get_ipaddr().c_str(), &server_address.sin_addr);

    // create a UDP socket
	uwe::socket sock{AF_INET, SOCK_DGRAM, 0};

	sock.bind((struct sockaddr *)&server_address, sizeof(server_address));

	// socket address used to store client address
	struct sockaddr_in client_address;
	size_t client_address_len = 0;

	char buffer[sizeof(chat::chat_message)];
    DEBUG("Entering server loop\n");
    bool exit_loop = false;
	for (;!exit_loop;) {
        int len = sock.recvfrom(
			buffer, sizeof(buffer), 0, (struct sockaddr *)&client_address, &client_address_len);

      
        // DEBUG("Received message:\n");
        if (len == sizeof(chat::chat_message)) {
            // handle incoming packet
            chat::chat_message * message = reinterpret_cast<chat::chat_message*>(buffer);
            auto type = static_cast<chat::chat_type>(message->type_);
            std::string username{(const char*)&message->username_[0]};
            std::string msg{(const char*)&message->message_[0]};

            if (is_valid_type(type)) {
                DEBUG("handling msg type %d\n", type);
                // valid type, so dispatch message handler
                handle_messages[type](online_users, username, msg, client_address, sock, exit_loop);
            }
        }
        else {
            DEBUG("Unexpected packet length\n");
        }
    }
}

/**
 * @brief entry point for chat server application
*/
int main(void) { 
    // Set server IP address
    uwe::set_ipaddr("192.168.1.11");
    server();

    return 0;
}