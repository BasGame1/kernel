#include "../../screen/screen.c"
#include "../../utils/parse_number.c"

int main(int argc, char **argv) {
   if (argc != 2) {
      print("Usage: round(int number, int page)");
   } else {
      int number = (int) parse_number(argv[0]);
      int page = (int) parse_number(argv[1]);
      int final_number = ((number + page) & 0xFFFFF000);
      print(final_number);
   }// if else
   return 0;
}// round
