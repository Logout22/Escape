#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cinttypes>
#include <cstring>

#include <arpa/inet.h>

using namespace std;

class PNGException : public std::exception {
    public:
        virtual char const *what() const throw();
};

class PNGCorruptHeaderException : public PNGException {
    public:
        char const *what() const throw();
};

class PNGCRCMismatchException : public PNGException {
    private:
        string blocktype;
    public:
        PNGCRCMismatchException(string blt) : blocktype(blt) {}
        char const *what() const throw();
};

size_t const PNG_HDRLEN = 8;
uint8_t const PNG_HDR[] = {
    0x89, 0x50, 0x4E, 0x47,
    0x0D, 0x0A, 0x1A, 0x0A,
};
uint32_t const PNG_CRC_TABLE[] = {
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    0x00000000, 0xEE0E612C, 0x076DC419, 0xE963A535, 0x0EDB8832, 0xE0D5E91E, 0x09B64C2B, 0xE7B82D07,
    0x00000000, 0x990951BA, 0xE963A535, 0x79DCB8A4, 0x09B64C2B, 0x90BF1D91, 0xF3B97148, 0x6DDDE4EB,
    0x00000000, 0x076DC419, 0x0EDB8832, 0x09B64C2B, 0x1DB71064, 0x1ADAD47D, 0x136C9856, 0x14015C4F,
    0x00000000, 0x706AF48F, 0xE0D5E91E, 0x90BF1D91, 0x1ADAD47D, 0x646BA8C0, 0xFA0F3D63, 0xA2677172,
    0x00000000, 0xE963A535, 0x09B64C2B, 0xF3B97148, 0x136C9856, 0xFA0F3D63, 0x3C03E4D1, 0xDBBBC9D6,
    0x00000000, 0x9E6495A3, 0xE7B82D07, 0x6DDDE4EB, 0x14015C4F, 0xA2677172, 0xDBBBC9D6, 0x51DE003A,
    0x00000000, 0x0EDB8832, 0x1DB71064, 0x136C9856, 0x3B6E20C8, 0x35B5A8FA, 0x26D930AC, 0x2802B89E,
    0x00000000, 0x79DCB8A4, 0xF3B97148, 0x8A65C9EC, 0x3C03E4D1, 0x45DF5C75, 0xCFBA9599, 0xB6662D3D,
    0x00000000, 0xE0D5E91E, 0x1ADAD47D, 0xFA0F3D63, 0x35B5A8FA, 0xC8D75180, 0x2F6F7C87, 0x9FBFE4A5,
    0x00000000, 0x97D2D988, 0xF4D4B551, 0x4C69105E, 0x32D86CE3, 0xB8BDA50F, 0x98D220BC, 0x086D3D2D,
    0x00000000, 0x09B64C2B, 0x136C9856, 0x3C03E4D1, 0x26D930AC, 0x2F6F7C87, 0x7807C9A2, 0x6C0695ED,
    0x00000000, 0x7EB17CBD, 0xFD62F97A, 0xA50AB56B, 0x21B4F4B5, 0x01DB7106, 0x91646C97, 0xFCB9887C,
    0x00000000, 0xE7B82D07, 0x14015C4F, 0xDBBBC9D6, 0x2802B89E, 0x9FBFE4A5, 0x6C0695ED, 0xA3BC0074,
    0x00000000, 0x90BF1D91, 0xFA0F3D63, 0x45DF5C75, 0x2F6F7C87, 0xE10E9818, 0x8BBEB8EA, 0x346ED9FC,
    0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC, 0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
    0x00000000, 0x6AB020F2, 0xD56041E4, 0xBFD06116, 0x71B18589, 0x1B01A57B, 0xA4D1C46D, 0xCE61E49F,
    0x00000000, 0xF3B97148, 0x3C03E4D1, 0xCFBA9599, 0x7807C9A2, 0x8BBEB8EA, 0x44042D73, 0xB7BD5C3B,
    0x00000000, 0x84BE41DE, 0xD20D85FD, 0x5F058808, 0x7F6A0DBB, 0xFBD44C65, 0xBE0B1010, 0x9DD277AF,
    0x00000000, 0x1ADAD47D, 0x35B5A8FA, 0x2F6F7C87, 0x6B6B51F4, 0x4ADFA541, 0x5EDEF90E, 0xE40ECF0B,
    0x00000000, 0x6DDDE4EB, 0xDBBBC9D6, 0xB6662D3D, 0x6C0695ED, 0x346ED9FC, 0xB7BD5C3B, 0x6906C2FE,
    0x00000000, 0xF4D4B551, 0x32D86CE3, 0x98D220BC, 0x65B0D9C6, 0xAA0A4C5F, 0xEAD54739, 0x10DA7A5A,
    0x00000000, 0x83D385C7, 0xDCD60DCF, 0x06B6B51F, 0x62DD1DDF, 0xC90C2086, 0x0D6D6A3E, 0xA1D1937E,
    0x00000000, 0x136C9856, 0x26D930AC, 0x7807C9A2, 0x4DB26158, 0x5EDEF90E, 0xF00F9344, 0xD80D2BDA,
    0x00000000, 0x646BA8C0, 0xC8D75180, 0xE10E9818, 0x4ADFA541, 0x2EB40D81, 0x196C3671, 0x4669BE79,
    0x00000000, 0xFD62F97A, 0x21B4F4B5, 0x91646C97, 0x4369E96A, 0x03B6E20C, 0xF9B9DF6F, 0x220216B9,
    0x00000000, 0x8A65C9EC, 0xCFBA9599, 0x1C6C6162, 0x44042D73, 0x73DC1683, 0x38D8C2C4, 0xB5D0CF31,
    0x00000000, 0x14015C4F, 0x2802B89E, 0x6C0695ED, 0x5005713C, 0xE40ECF0B, 0xD80D2BDA, 0x9C0906A9,
    0x00000000, 0x63066CD9, 0xC60CD9B2, 0xF50FC457, 0x5768B525, 0x8708A3D2, 0x316E8EEF, 0x0CB61B38,
    0x00000000, 0xFA0F3D63, 0x2F6F7C87, 0x8BBEB8EA, 0x5EDEF90E, 0x196C3671, 0xCC0C7795, 0x68DDB3F8,
    0x00000000, 0x8D080DF5, 0xC1611DAB, 0x15DA2D49, 0x59B33D17, 0x67DD4ACC, 0x2BB45A92, 0xFF0F6A70,
};


char const *PNGException::what() const throw() {
    return "Error loading PNG image: ";
}

char const *PNGCorruptHeaderException::what() const throw() {
    string error = PNGException::what();
    error += "Corrupt header";
    return error.c_str();
}

char const *PNGCRCMismatchException::what() const throw() {
    string error = PNGException::what();
    error += "CRC of block " + blocktype + " wrong; block corrupt";
    return error.c_str();
}

/* adapted from RFC 2083 */
uint32_t crc(vector<uint8_t> &buf)
{
    uint32_t c = 0xffffffff;
    for (uint8_t elem : buf) {
        c = PNG_CRC_TABLE[(c ^ elem) & 0xff] ^ (c >> 8);
    }
    c ^= 0xffffffff;
    return c;
}

int main(int argc, char *argv[]) {
    if (argc < 2) return 0;

    try {
        ifstream png_file(argv[1], ios::in | ios::binary);

        char header[PNG_HDRLEN];
        png_file.read(header, 8);
        if (memcmp(header, PNG_HDR, PNG_HDRLEN) != 0) {
            throw PNGCorruptHeaderException();
        }

        while (!png_file.eof()) {
            uint32_t chunklen_net;
            png_file.read((char*) &chunklen_net, 4);
            uint32_t chunk_len = ntohl(chunklen_net);

            char chunk_type[4];
            png_file.read(chunk_type, 4);

            vector<uint8_t> chunk_data(chunk_len);
            png_file.read((char*) &chunk_data[0], chunk_len);

            uint32_t chunkcrc_net;
            png_file.read((char*) &chunkcrc_net, 4);
            uint32_t chunk_crc = ntohl(chunkcrc_net);
            if (chunk_crc != crc(chunk_data)) {
                throw PNGCRCMismatchException(chunk_type);
            }
        }
    } catch(const exception &e) {
        cout << "Exception: " << e.what() << endl;
    }
}

