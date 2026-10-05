#include "bplustree.h"

#include <iostream>
#include <exception>

int main() {

    try {

        std::cout
            << "Creating B+ tree index...\n\n";

        bptree::BPlusTree tree(
            "index.db",
            true
        );

        tree.printDesign();

        tree.printHeader();

        std::cout
            << "\nB+ tree index.db created successfully.\n";

        std::cout
            << "B+ tree parameter n = "
            << tree.n()
            << "\n";

        return 0;

    }
    catch (const std::exception& e) {

        std::cerr
            << "ERROR: "
            << e.what()
            << "\n";

        return 1;
    }
}