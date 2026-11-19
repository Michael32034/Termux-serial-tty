/*
 *  @file ptyserial.cpp
 *  This example attaches a pseudo-tty to a given USB-UART device
 */
/* This file is part of USBUART Library. http://hutorny.in.ua/projects/usbuart
 *
 * Copyright (C) 2016 Eugene Hutorny <eugene@hutorny.in.ua>
 *
 * The USBUART Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License v2
 * as published by the Free Software Foundation;
 *
 * The USBUART Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the USBUART Library; if not, see
 * <http://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include <bits/getopt.h>
#include <cstddef>
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <pty.h>
#include <fcntl.h>
#include <string>
#include <sys/wait.h>
#include "usbuart.h"

static bool terminated = false;

static void doexit(int signal) {
	terminated = true;
}

void show_err(int err) {
	fprintf(stderr,"err(%d)==%s\n", err, strerror(err));
}
using namespace usbuart;
using namespace std::chrono;
namespace fs = std::filesystem;

static inline bool is_good(int status) noexcept {
	return status == status_t::all_ok;
}

static inline bool is_usable(int status) noexcept {
	return	status == (status_t::usb_dev_ok | status_t::read_pipe_ok)  ||
			status == (status_t::usb_dev_ok | status_t::write_pipe_ok) ||
			status ==  status_t::all_ok;
}

void create_symlink(char* parent, fs::path link) {
  if (!link.empty()) {
    	fs::create_symlink(parent, link);
  }
}

void close_symlink(fs::path link) {
  if (fs::exists(link)) {
    	fs::remove(link);
  }
}

int main(int argc, char** argv) {
	channel chnl = bad_channel;
	int master, slave;

	if( argc < 1 ) {
		fprintf(stderr,"Usage: termux-usb -e '%s  <command> [command arguments]' <usb device address>\n",argv[0]);
		return -1;
	}
	const char* usb_fd_arg_str = argv[argc-1];
	int usb_dev_fd = -1;
    try {
        usb_dev_fd = std::stoi(usb_fd_arg_str);
    } catch (const std::invalid_argument& ia) {
        fprintf(stderr,"Error: Invalid USB file descriptor argument %s. Not a number.\n",usb_fd_arg_str);
        return EXIT_FAILURE;
    } catch (const std::out_of_range& oor) {
        fprintf(stderr,"Error: USB file descriptor argument %s out of range.\n",usb_fd_arg_str);
        return EXIT_FAILURE;
    }

    if (usb_dev_fd < 0) { 
        fprintf(stderr,"Error: Invalid USB file descriptor value parsed: %d (must be non-negative).\n",usb_dev_fd);
        return EXIT_FAILURE;
    }
	eia_tia_232_info uart_args = _115200_8N1n;
	fs::path pty_link;

	int opt;
	while ((opt = getopt(argc, argv, "b:s:")) != -1) {
		switch (opt) {
			case 'b':
				switch (std::stoi(optarg)) {
					case 115200:
					    break;
					case 19200:
						uart_args = _19200_8N1n;
					    break;
					case 9600:
						uart_args = _9600_8N1n;
					    break;
					default:
						perror("Wrong baudrate");
						return EXIT_FAILURE;
				}
				break;
			case 's':
				pty_link = fs::absolute(optarg);
				break;
		}
	}


  int created = openpty(&master, &slave, NULL, NULL, NULL);
  if (created < 0) {
	perror("Could not create pty");
	return EXIT_FAILURE;
  }
  create_symlink(ptsname(master), pty_link);
// Parent process
  fprintf(stderr, "PTY created: %s\n",ptsname(master));
  chnl.fd_read = master;
  chnl.fd_write = master;

  context::setloglevel(loglevel_t::info);

  context ctx;
  int res, status = 0;
  res = ctx.attach(usb_dev_fd, 0, chnl, uart_args);
  if( res ) {
	fprintf(stderr,"Error %d attaching fd %x\n",
			-res, usb_dev_fd);
	close_symlink(pty_link);
	return -res;
  }


	signal(SIGINT, doexit);
	signal(SIGQUIT, doexit);
    signal(SIGCHLD, doexit);

	int count_down = 10;
	int timeout = 0;

	while(!terminated && (res=ctx.loop(timeout)) >= -error_t::no_channel) {
		if( ! is_usable(status = ctx.status(chnl)) ) break;
		if( res == -error_t::no_channel || ! is_good(status) ) {
			timeout = 100;
			fprintf(stderr,"Channel error:%d. Status=%d.\n",res,status);
			if( --count_down <= 0 ) break;
		}
	}
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGCHLD, SIG_DFL);

	fprintf(stderr,"USB status %d res %d\n", status, res);
	ctx.close(chnl);
	res = ctx.loop(1000);
	if( res < -error_t::no_channel ) {
		fprintf(stderr,"Terminated with error %d\n",-res);
	} else
		res = 0;

	close_symlink(pty_link);
	close(master);

	return res;
}
