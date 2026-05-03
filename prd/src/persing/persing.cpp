#include <string>
#include "persing_conf.hpp"

void parsing_args(int argc, char **argv, Conf &conf)
{
    validate_arguments(argc, argv);

    conf.read_file(argv[1]);
    conf.check_directive();
    conf.init_file_pos();
    conf.stock_data();
    conf.check_data();
}
