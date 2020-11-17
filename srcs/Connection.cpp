/* ************************************************************************** */
/*                                                                            */
/*                                                        ::::::::            */
/*   Connection.cpp                                     :+:    :+:            */
/*                                                     +:+                    */
/*   By: sam <sam@student.codam.nl>                   +#+                     */
/*                                                   +#+                      */
/*   Created: 2020/10/03 15:26:44 by sam           #+#    #+#                 */
/*   Updated: 2020/11/08 15:36:30 by tuperera      ########   odam.nl         */
/*                                                                            */
/* ************************************************************************** */

#include "Connection.hpp"
#include <iostream>
#include <zconf.h>
#include "ResponseHandler.hpp"
#include "libftGnl.hpp"

Connection::Connection() : _serverAddr(), _master(), _readFds() {
	FD_ZERO(&_master);
	FD_ZERO(&_readFds);
	_connectionFd = 0;
	_fdMax = 0;
}

Connection::~Connection() {
	for (std::vector<Server>::iterator it = _servers.begin(); it != _servers.end(); it++) {
		close(it->getSocketFd());
	}
	close(_connectionFd);
}

Connection::Connection(const Connection &obj) : _connectionFd(), _fdMax(), _serverAddr(), _master(), _readFds() {
	*this = obj;
}

Connection& Connection::operator= (const Connection &obj) {
	if (this != &obj) {
		this->_connectionFd = obj._connectionFd;
		this->_connectionFd = obj._connectionFd;
		this->_fdMax = obj._fdMax;
		this->_serverAddr = obj._serverAddr;
		this->_master = obj._master;
		this->_readFds = obj._readFds;
		this->_rawRequest = obj._rawRequest;
		this->_parsedRequest = obj._parsedRequest;
		this->_servers = obj._servers;
		this->_serverMap = obj._serverMap;
	}
	return *this;
}

void Connection::setUpConnection() {
	int socketFd;
	int opt = 1;
	for (std::vector<Server>::iterator it = _servers.begin(); it != _servers.end(); it++) {
		ft_memset(&_serverAddr, 0, sizeof(_serverAddr)); // Clear struct from prev setup
		if (!(socketFd = socket(AF_INET, SOCK_STREAM, 0)))
			throw std::runtime_error(strerror(errno));

		if (setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
			throw std::runtime_error(strerror(errno));
		}

		// Fill struct with info about port and ip
		_serverAddr.sin_family = AF_INET; // ipv4
		if (it->gethost() == "localhost") {
			_serverAddr.sin_addr.s_addr = INADDR_ANY; // choose local ip for me
		}
		else {
			_serverAddr.sin_addr.s_addr = inet_addr(it->gethost().c_str());
		}
		_serverAddr.sin_port = htons(it->getport()); // set port (htons translates host bit order to network order)

		// Associate the socket with a port and ip
		if (bind(socketFd, (struct sockaddr*) &_serverAddr, sizeof(_serverAddr)) < 0) {
			std::cout << "Cannot bind: " << errno << std::endl;
			throw std::runtime_error(strerror(errno));
		}

		// Start listening for connections on port set in sFd, max BACKLOG waiting connections
		if (listen(socketFd, BACKLOG))
			throw std::runtime_error(strerror(errno));
		it->setSocketFd(socketFd);
	}
}

void Connection::startListening() {
	RequestParser					requestParser;
	ResponseHandler					responseHandler;
	std::string						response;
	std::map<int, Server>::iterator	serverMapIt;
	std::map<int, Server> 			serverConnections;

	for (std::vector<Server>::iterator it = _servers.begin(); it != _servers.end(); it++) {
		// Add the listening socket to the master list
		FD_SET(it->getSocketFd(), &_master);
		FD_SET(0, &_master); // Add stdin so we always listen to cl commands
		// Keep track of the biggest fd on the list
		_fdMax = it->getSocketFd();
		_serverMap.insert(std::make_pair(it->getSocketFd(), *it)); // Keep track of which socket is for which server obj
	}
	std::cout << "Waiting for connections..." << std::endl;
	while (true) {
		_readFds = _master;
		if (select(_fdMax + 1, &_readFds, NULL, NULL, NULL) == -1)
			throw std::runtime_error(strerror(errno));
		// Go through existing connections looking for data to read
		for (int fd = 0; fd <= _fdMax; fd++) {
			if (FD_ISSET(fd, &_readFds)) { // Returns true if fd is active
				if (fd == 0) {
					std::string clInput;
					std::getline(std::cin, clInput);
					std::cout << "CLI input: " << clInput << std::endl;
				}
				else if ((serverMapIt = _serverMap.find(fd)) != _serverMap.end()) { // This means there is a new connection waiting to be accepted
					serverConnections.insert(std::make_pair(addConnection(serverMapIt->second.getSocketFd()), serverMapIt->second));
				}
				else { // Handle request & return response
					receiveRequest(fd);
					_parsedRequest = requestParser.parseRequest(_rawRequest);
					_parsedRequest.server = serverConnections[fd];
					response = responseHandler.handleRequest(_parsedRequest);
					sendReply(response, fd);
					closeConnection(fd);
					serverConnections.erase(fd);
				}
			}
		}
	}
}

int Connection::addConnection(const int &socketFd) {
	struct sockaddr_storage their_addr = {}; // Will contain info about connecting client
	socklen_t addr_size;

	// Accept one connection from backlog
	addr_size = sizeof their_addr;
	if ((_connectionFd = accept(socketFd, (struct sockaddr*)&their_addr, &addr_size)) < 0)
		throw std::runtime_error(strerror(errno));
	FD_SET(_connectionFd, &_master); // Add new connection to the set
	if (_connectionFd > _fdMax)
		_fdMax = _connectionFd;
	std::cout << "New client connected" << std::endl;
	return _connectionFd;
}

void Connection::receiveRequest(const int& fd) {
	char buf[BUFLEN];
	std::string request;
	int bytesReceived;
	// Loop to receive complete request, even if buffer is smaller
	request.clear();
	ft_memset(buf, 0, BUFLEN);
	do {
		bytesReceived = recv(fd, buf, BUFLEN - 1, 0);
		if (bytesReceived == -1)
			throw std::runtime_error(strerror(errno));
		request += buf;
		ft_memset(buf, 0, BUFLEN);
	} while (bytesReceived == BUFLEN - 1);
	_rawRequest = request;
	std::cout << "REQUEST == \n" << _rawRequest << std::endl;
}

void Connection::sendReply(const std::string& msg, const int& fd) const {
	if ((send(fd, msg.c_str(), msg.length(), 0) == -1))
		throw std::runtime_error(strerror(errno));
}

void Connection::closeConnection(const int& fd) {
	// Closing connection after response has been send
	close(fd);
	FD_CLR(fd, &_master);
}

void Connection::setServers(const std::vector<Server>& servers) {
	_servers = servers;
}



















void Connection::startServer() {
	loadConfiguration();
	setUpConnection();
	startListening();
}

void Connection::loadConfiguration() {
	_manager = parse(_configPath);
	for (size_t i = 0; i < _manager.size(); i++)
		std::cout << _manager[i];

	setServers(_manager.getServers());
}

void Connection::handleCLI(std::string input) {
	if (input == "exit") {
		std::cout << "Shutting down..." << std::endl;
		exit(0);
	}
	else if (input == "restart") {
		std::cout << "Restarting server..." << std::endl;
//		stopServer();
		startServer();
	}
	else if (input == "help") {
		std::cout << "These are common Git commands used in various situations:\n"
					 "\n"
					 "start a working area (see also: git help tutorial)\n"
					 "   clone             Clone a repository into a new directory\n"
					 "   init              Create an empty Git repository or reinitialize an existing one\n"
					 "\n"
					 "work on the current change (see also: git help everyday)\n"
					 "   add               Add file contents to the index\n"
					 "   mv                Move or rename a file, a directory, or a symlink\n"
					 "   restore           Restore working tree files\n"
					 "   rm                Remove files from the working tree and from the index\n"
					 "   sparse-checkout   Initialize and modify the sparse-checkout" << std::endl;
	}
	else {
		std::cout << "Command \"" << input << "\" not found. Type \"help\" for available commands" << std::endl;
	}
}

