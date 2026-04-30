#include <string>
#include "webserver.hpp"

void parsing(int argc, char **argv, Conf &conf)
{
    validate_arguments(argc, argv);

    conf.read_file(argv[1]);
    conf.check_directive();
    conf.stock_data();
    conf.print_raw_data();
}