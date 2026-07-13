
#include <llvm/IR/Value.h>
#include <map>

// The laxer returns tokens [0-255] if it is an unknown character, otherwise one
// of these for known things

enum class Token {
  tok_eof,
  tok_def,
  tok_extern,
  tok_identifier,
  tok_number,
  tok_char
};

extern std::string IdentifierStr;
extern char ThisChar;
extern double NumVal;
extern std::map<char, int> BinopPrecedence;
extern Token CurTok;


Token gettok();
Token getNextToken();
void InitializeModule();
void MainLoop();