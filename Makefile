################################################################################
# Makefile                                                                     #
#                                                                              #
# Description: This file contains the make rules for Recitation 1.             #
#                                                                              #
# Authors: Athula Balachandran <abalacha@cs.cmu.edu>,                          #
#          Wolf Richter <wolf@cs.cmu.edu>                                      #
#                                                                              #
################################################################################

default: lisod echo_client

lisod:
	@gcc liso_server.c es_error_handler.c liso_hash.c http_parser.c liso_file_io.c liso_logger.c liso_select_engine.c es_connection.c -o lisod -Wall -Werror -l ssl

echo_client:
	@gcc echo_client.c -o echo_client -Wall -Werror

clean:
	@rm lisod echo_client
