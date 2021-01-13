/* ************************************************************************** */
/*                                                                            */
/*                                                        ::::::::            */
/*   main.cpp                                           :+:    :+:            */
/*                                                     +:+                    */
/*   By: sam <sam@student.codam.nl>                   +#+                     */
/*                                                   +#+                      */
/*   Created: 2020/10/02 16:00:45 by sam           #+#    #+#                 */
/*   Updated: 2020/10/13 16:24:06 by pde-bakk      ########   odam.nl         */
/*                                                                            */
/* ************************************************************************** */

#include "Connection.hpp"
#include <iostream>
#include <signal.h>

void shutdown(int n) {
	std::cout << std::endl << "Shutting down..." << std::endl;
	exit(n);
}

int main(int argc, char **argv) {
	Connection serverProcess(argv[1]);
	(void)argc;

	signal(SIGINT, shutdown);

	try {
		serverProcess.startServer();
	}
	catch (std::runtime_error &e) {
		std::cout << e.what() << std::endl;
	}

	return 0;
}
