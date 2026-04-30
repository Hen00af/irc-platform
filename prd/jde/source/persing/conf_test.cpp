#include "webserver.hpp"
#include "../logging/logging.hpp"
#include <iostream>
#include <fstream>

int main(int ac, char **av) {
    Conf conf;
    if (ac != 2 || av == NULL || av[1] == NULL) {
        std::cerr << "Usage: ./conf_test <config_file>" << std::endl;
        return 1;
    }

    std::string config_path = av[1];

    std::ifstream ifs(config_path.c_str());
    if (!ifs.is_open()) {
        std::cerr << "Error: Could not open config file: " << config_path << std::endl;
        return -1;
    }
    ifs.close(); // 存在確認ができたら一度閉じる（conf.read_file内で再度開くため）

    try {
        // 2. ファイルの読み込み
        std::cout << "Reading file..." << std::endl;
        conf.read_file(config_path);

        // 3. ディレクティブのチェックとバリデーション
        std::cout << "Checking directives..." << std::endl;
        conf.check_directive();

        std::cout << "Initializing parse positions..." << std::endl;
        conf.init_file_pos();

        // 4. データの格納（メモリへのストック）
        std::cout << "Stocking data..." << std::endl;
        conf.stock_data();

        std::cout << "Validating parsed data..." << std::endl;
        conf.check_data();

        // 5. 結果の表示（正しく格納されたか確認）
        std::cout << "\n--- Parsed Configuration Data ---" << std::endl;
        print_all_data(conf);

    } catch (const std::exception& e) {
        // 自作例外クラス（ArgvErr, DirWrongなど）をキャッチして表示
        std::cerr << "\n[Config Error] " << e.what() << std::endl;
        return 1;
    }

    std::cout << "\nTest completed successfully!" << std::endl;
    return 0;
}
