#ifndef FILEUTILS_H
#define FILEUTILS_H

#include <vector>
#include <string>


namespace FileUtils
{
    // read the file lines from a file path.
    //
    // @param filePath file path to read from.
    // @param resultFileLines stored read file lines.
    void readFileLines(const char *filePath, std::vector<std::string>& resultFileLines);

    // read the file characters from a file path.
    //
    // @param filePath file path to read from.
    // @param binaryFile if the file should be read as a binary file.
    // @param resultFileChars stored read file chars.
    void readFileChars(const char *filePath, bool binaryFile, std::vector<char>& resultFileChars);
}

#endif  // FILEUTILS_H
