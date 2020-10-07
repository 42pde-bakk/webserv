/* ************************************************************************** */
/*                                                                            */
/*                                                        ::::::::            */
/*   Connection.hpp                                     :+:    :+:            */
/*                                                     +:+                    */
/*   By: sam <sam@student.codam.nl>                   +#+                     */
/*                                                   +#+                      */
/*   Created: 2020/10/03 15:26:41 by sam           #+#    #+#                 */
/*   Updated: 2020/10/03 15:26:41 by sam           ########   odam.nl         */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <iostream>
#include <stdexcept>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <zconf.h>

#define PORT 8080	// The port we will be listening on TODO: set through config file
#define BACKLOG 5	// how many pending connections queue will hold
#define BUFLEN 8192

class Connection {
	int socketFd, connectionFd, fdMax;
	struct sockaddr_in server; // Will contain info about port and ip
	fd_set master;    // master file descriptor list
	fd_set readFds;  // temp file descriptor list for select()
	std::string _rawRequest;
	void addConnection();
	void receiveRequest();
	void sendReply(const std::string &msg) const;
	void closeConnection(int fd);
public:
	Connection();
	virtual ~Connection();
	void setUpConnection();
	void startListening();
};


#endif //CONNECTION_HPP
