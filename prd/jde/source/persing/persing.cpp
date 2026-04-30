#include <string>

void parse_basic(int argc, char **argv) {
    if (argc != 2)  
        throw ArgvErr();

    std::ifstream file(argv[1]);

    if (!file) 
        throw  ArgvErr();
    else {
        std::string name = std::string(argv[1]);

        if (name.find(".conf") == std::string::nops) // nops == -1 in size_t
            throw ArgvErr();
    }
}

void parsing(int argc, char **argv, Conf &data) {
    parse_basic(argc, argv);
    data.read_file(argv[i]);
    data.init_file_pos();
    data.check_directive();
    data.stocl_data();
    data.check_data();
}
