#include <iostream>
#include <cstdio>
#include <vector>
#include <string>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <utility>
#include <map>
#include <set>
#include <memory> 

using namespace std ; 

static int gTestNum = -1 ;

enum TokenType { LEFT_PAREN, RIGHT_PAREN, INT, STRING, DOT, FLOAT, NIL, T, QUOTE, SYMBOL, DEFAULT, EXIT, ERROR, ERROROBJ };
enum NodeType { ATOM, CONS, LAMBDA };

bool EOFExit = false ;

struct Token {
	
	string tokenStr = "\0" ;
	TokenType type = DEFAULT ;
	int Ivalue = -1 ;
	double Fvalue = -1 ;
	int line = -1 ;
	int firstcolumn = -1 ;
	
};

struct ASTNode {
	NodeType type ;
	virtual ~ASTNode() {}
};

struct ATOMNode : public ASTNode {
	string value ;
	TokenType tokentype ;
	
	ATOMNode() : value( "" ), tokentype( DEFAULT ) {type = ATOM ;}
	ATOMNode( const string& val, TokenType Ttype ) : value( val ), tokentype( Ttype ) { type = ATOM ; }
};

struct ConsNode : public ASTNode {
    shared_ptr<ASTNode> left;
    shared_ptr<ASTNode> right;
    
    ConsNode( shared_ptr<ASTNode> l, shared_ptr<ASTNode> r = nullptr ) : left(l), right(r) { this->type = CONS ; }
};

struct LambdaNode : public ASTNode {
    shared_ptr<ASTNode> variables;
    shared_ptr<ASTNode> functions;
    string value ;
    
    LambdaNode( shared_ptr<ASTNode> v, shared_ptr<ASTNode> f ) : variables(v), functions(f) { this->type = LAMBDA ; value = "lambda" ; }
    LambdaNode( shared_ptr<ASTNode> v, shared_ptr<ASTNode> f, string name ) : variables(v), functions(f), value(name) { this->type = LAMBDA ; }
};

bool IsBuiltIn( string name ) {
	// connect
	if (name == "cons" || name == "list")
		return true;
		
		    // car cdr
	if (name == "car" || name == "cdr")
		return true;
		
	// Predicates
	if (name == "atom?" || name == "pair?" || name == "list?" || name == "null?" ||
		name == "integer?" || name == "real?" || name == "number?" ||
		name == "string?" || name == "boolean?" || name == "symbol?")
		return true;
		
	// Arithmetic
	if (name == "+" || name == "-" || name == "*" || name == "/")
		return true;
		
	// Logic
	if (name == "not" || name == "and" || name == "or")
	    return true;
		
		    // Comparison
	if (name == ">" || name == ">=" || name == "<" || name == "<=" || name == "=")
		return true;
		
		    // String
	if (name == "string-append" || name == "string>?" || name == "string<?" || name == "string=?")
		return true;
		
		    // Equal
	if (name == "eqv?" || name == "equal?")
		return true;
		
	return false;
}
bool IsFunctionName( string name ) {
	return ( name == "quote" || name == "\'" || name == "define" || name == "if" || name == "cond" || name == "begin" || IsBuiltIn(name) || name == "clean-environment" )  ;
} // IsFunctionName()
		

int gLine = 1 ;    // the line-no of of the char that is yet to be read in
int gColumn = 1 ;  // the column-no of of the char that is yet to be read in

char gNextChar = '\0' ;    // the char we just read in
int gNextCharLine = -1 ;   // line-no of gNextChar (the char we just read in)
int gNextCharColumn = -1 ; // column-no of gNextChar (the char we just read in)

// ---------------------------------------error處理 start ----------------------------------------------------------

class ProjectException : public exception {
	public :
		virtual const char* what() const noexcept override {
			return "There is an exception in your project！\n" ;
		}
}; // class ProjectException

class UnExpectedTokenException : public ProjectException {
	private:
		int Line ;
		int Column ;
		string Str ;
	public:
		
		UnExpectedTokenException( int line, int column, string str ) : Line(line), Column(column), Str(str) {} 
		
		int getLine() const {
			return Line ;
		} // getLine()
		int getColumn() const {
			return Column ;
		} // getColumn()
		string getStr() const {
			return Str ; 
		} // getStr()
		
		const char* what() const noexcept override {
			return "ERROR (unexpected token) :" ;
		}
	
};

class NoClosingQuoteException : public ProjectException {
	private:	
		int Line ;
		int Column ;

	public:
		
		NoClosingQuoteException( int line, int column ) : Line(line), Column(column) {} 
		
		int getLine() const {
			return Line ;
		} // getLine()
		
		int getColumn() const {
			return Column ;
		} // getColumn()

		
		const char* what() const noexcept override {
			return "ERROR (no closing quote) :" ;
		} // what
	
};

class NoMoreInputException : public ProjectException {
	private :
		shared_ptr<ASTNode> node ;
	public:
		NoMoreInputException() : node( nullptr ) {}
		NoMoreInputException( shared_ptr<ASTNode> Node ) : node( Node ) {}
		
		const char* what() const noexcept override {
			return "ERROR (no more input) : END-OF-FILE encountered" ;
		}
		
    	shared_ptr<ASTNode> GetNode() const {
    		return node ;
		} // GetNode()
};

class MissingRightParenException : public ProjectException {
	private:
		int Line ;
		int Column ;
		string Str ;
	public:
		
		MissingRightParenException( int line, int column, string str ) : Line(line), Column(column), Str(str) {} 
		
		int getLine() const {
			return Line ;
		} // getLine()
		int getColumn() const {
			return Column ;
		} // getColumn()
		string getStr() const {
			return Str ; 
		} // getStr()
		
		const char* what() const noexcept override {
			return "ERROR (unexpected token) :" ;
		}
	
};

class ExitException : public ProjectException {
	public:
		const char* what() const noexcept override {
			return "ExitException！" ;
		}
	
};

class WrongNumberOfArgumentException : public ProjectException {
	
	private:
		string FuncName ;
		string mag ; 
	public :
		WrongNumberOfArgumentException ( string funcName ) : FuncName( funcName ),  mag("ERROR (incorrect number of arguments) : " + funcName) {} 
		
		string getFuncName() const {
			return FuncName ;
		} // getFuncName()
		 
		const char* what() const noexcept override {
			return mag.c_str() ;
		}
		
}; 

// lambda
class NoReturnValueException : public ProjectException {
	private :
		shared_ptr<ASTNode> node ;
		bool classified ;
	public :
		
//		NoReturnValueException() : node(nullptr) {}
		NoReturnValueException( shared_ptr<ASTNode> Node ) : node( Node ), classified( false ) {}
		NoReturnValueException( shared_ptr<ASTNode> Node, bool c ) : node( Node ), classified( c ) {}
		const char* what() const noexcept override {
			return "ERROR (no return value) : " ;
		}
		
		bool GetClassified() {
			return classified ;
		} // GetClassified()
		
		void MarkClassified() {
			classified = true ;
		} // MarkClassified()
		
    	shared_ptr<ASTNode> GetNode() const {
    		return node ;
		} // GetNode()
}; 

// lambda
class UnboundParameterException : public ProjectException {
	private :
		shared_ptr<ASTNode> node ;
	public :
		
		UnboundParameterException( shared_ptr<ASTNode> Node ) : node( Node ) {}
		
		const char* what() const noexcept override {
			return "ERROR (unbound parameter) : " ;
		}
		
    	shared_ptr<ASTNode> GetNode() const {
    		return node ;
		} // GetNode()
}; 

// lambda
class UnboundConditionException : public ProjectException {
	private :
		shared_ptr<ASTNode> node ;
	public :	
	
		UnboundConditionException( shared_ptr<ASTNode> Node ) : node( Node ) {}
		
		const char* what() const noexcept override {
			return "ERROR (unbound condition) : " ;
		}
		
    	shared_ptr<ASTNode> GetNode() const {
    		return node ;
		} // GetNode()
};

// lambda
class UnboundTestConditionException : public ProjectException {
	private :
		shared_ptr<ASTNode> node ;
	public :	
	
		UnboundTestConditionException( shared_ptr<ASTNode> Node ) : node( Node ) {}
		
		const char* what() const noexcept override {
			return "ERROR (unbound test-condition) : " ;
		}
		
    	shared_ptr<ASTNode> GetNode() const {
    		return node ;
		} // GetNode()
};

class NonListException : public ProjectException {
	private :
		shared_ptr<ASTNode> node ;
	public :
		NonListException() : node(nullptr) {}
		NonListException ( shared_ptr<ASTNode> Node ) : node( Node ) {}
		
		const char* what() const noexcept override {
			return "ERROR (non-list) : " ;
		}
    	void SetNode(shared_ptr<ASTNode> n) {
        	node = n;
    	}
    	shared_ptr<ASTNode> GetNode() const {
    		return node ;
		} 

}; 

class DefineFormatException : public ProjectException {

	public :
		const char* what() const noexcept override {
			return "ERROR (DEFINE format) : " ;
		}
		
}; 

class CondFormatException : public ProjectException {
	private :
		shared_ptr<ASTNode> node ;
	public :
		CondFormatException() : node(nullptr) {}
		CondFormatException(shared_ptr<ASTNode> Node) : node(Node) {} 
		const char* what() const noexcept override {
			return "ERROR (COND format) : " ;
		}
    	shared_ptr<ASTNode> GetNode() const {
    		return node ;
		} 
}; 

class LetFormatException : public ProjectException {
	private :
		shared_ptr<ASTNode> node ;
	public :
		LetFormatException() : node(nullptr) {}
		LetFormatException(shared_ptr<ASTNode> Node) : node(Node) {} 
		const char* what() const noexcept override {
			return "ERROR (LET format) : " ;
		}
    	shared_ptr<ASTNode> GetNode() const {
    		return node ;
		} 
}; 

class SetFormatException : public ProjectException {
	private :
		shared_ptr<ASTNode> node ;
	public :
		SetFormatException() : node(nullptr) {}
		SetFormatException(shared_ptr<ASTNode> Node) : node(Node) {} 
		const char* what() const noexcept override {
			return "ERROR (SET! format) : " ;
		}
    	shared_ptr<ASTNode> GetNode() const {
    		return node ;
		} 
}; 

class LambdaFormatException : public ProjectException {
	private :
		shared_ptr<ASTNode> node ;
	public :
		LambdaFormatException() : node(nullptr) {}
		LambdaFormatException(shared_ptr<ASTNode> Node) : node(Node) {} 
		const char* what() const noexcept override {
			return "ERROR (LAMBDA format) : " ;
		}
    	shared_ptr<ASTNode> GetNode() const {
    		return node ;
		} 
}; 

class LevelOfDefineException : public ProjectException {

	public :
		const char* what() const noexcept override {
			return "ERROR (level of DEFINE)" ;
		}
		
}; 

class LevelOfCleanException : public ProjectException {

	public :
		const char* what() const noexcept override {
			return "ERROR (level of CLEAN-ENVIRONMENT)" ;
		}
		
}; 

class LevelOfExitException : public ProjectException {

	public :
		const char* what() const noexcept override {
			return "ERROR (level of EXIT)" ;
		}
		
}; 

class UnboundSymbolException : public ProjectException {
	
	private:
		string Symbol ;
		string mag ; 
	public :
		UnboundSymbolException ( string symbol ) : Symbol( symbol ),  mag("ERROR (unbound symbol) : " + symbol) {} 
		
		string getSymbol() const {
			return Symbol ;
		} // getFuncName()
		 
		const char* what() const noexcept override {
			return mag.c_str() ;
		}
		
}; 

class AttemptToApplyNonFunctionException : public ProjectException {
	
	private:
		shared_ptr<ASTNode> Nfunc ;
		string msg ; 
	public :
		AttemptToApplyNonFunctionException ( shared_ptr<ASTNode> nfunc ) : Nfunc( nfunc ),  msg("ERROR (attempt to apply non-function) : ") {} 
		
		shared_ptr<ASTNode> getNfunc() const {
			return Nfunc ;
		} // getValue()
		 
		const char* what() const noexcept override {
			return msg.c_str() ;
		}
		
}; 

class IncorrectArgumentTypeException : public ProjectException {
	
	private:
		shared_ptr<ASTNode> Value ;
		string msg ; 
		string Function ;
	public :
		IncorrectArgumentTypeException ( shared_ptr<ASTNode> value, string fun ) : Value( value ), Function(fun), msg("ERROR (" + fun + " with incorrect argument type) : ") {} 
		
		shared_ptr<ASTNode> getValue() const {
			return Value ;
		} // getValue()
		 
		const char* what() const noexcept override {
			return msg.c_str() ;
		}
}; 

class DivisionByZeroException : public ProjectException {

	public :
		const char* what() const noexcept override {
			return "ERROR (division by zero) : /" ;
		}
		
}; 
// ---------------------------------------error處理 end ----------------------------------------------------------


bool GetNextChar ( char & ch, int & line, int & column ) {
	
    
  if ( !(cin.get( ch )) ) {
  	return false ;
  }
  	
  line = gLine ;
  column = gColumn ;

  if ( ch != '\n' ){
    gColumn = gColumn + 1 ;
  }
  else {
    gLine = gLine + 1 ;
    gColumn = 1 ;
  } // else  
//  cout << "gNextCharLine： " << gNextCharLine << " gNextCharColumn： " << gNextCharColumn << " gNextChar： " << gNextChar << endl ;
  return true ;

} // GetNextChar()

string GetTokenTypeName( TokenType type ) {
    switch ( type ) {
        case LEFT_PAREN: return "LEFT_PAREN";
        case RIGHT_PAREN: return "RIGHT_PAREN";
        case INT: return "INT";
        case STRING: return "STRING";
        case DOT: return "DOT";
        case FLOAT: return "FLOAT";
        case NIL: return "NIL";
        case T: return "T";
        case QUOTE: return "QUOTE";
        case SYMBOL: return "SYMBOL";
        case EXIT: return "EXIT" ;
        case ERROR: return "ERROR" ;
        case DEFAULT: return "DEFAULT";
        default: return "UNKNOWN";
    }
} // GetTokenTypeName

void PrintToken( Token token ) {
	
	string typeName = GetTokenTypeName( token.type ) ;
	
	if ( token.type == INT )
		printf( "%d ", token.Ivalue ) ;
	else if ( token.type == FLOAT )
		printf( "%.3f ", token.Fvalue ) ;
	else
		printf( "%s ", token.tokenStr.c_str() );
		
	printf( "%s %d %d\n", typeName.c_str(), token.line , token.firstcolumn ) ;
	
} // PrintToken()

bool IsATOMinNode( TokenType type ) {
	
	if ( type == INT || type == STRING || type == FLOAT || type == NIL || type == T || type == SYMBOL )
		return true ;
	
	return false ;
	
} // IsATOMinNode

// ---------------------------------------檢查字元strat--------------------------------------------------- 
bool IsWhiteSpace ( char ch ) {
	
	if ( ch == ' ' || ch == '\t' || ch == '\n' ) 
		return true ;
	else 
		return false ;
		
} // IsWhiteSpace()


bool IsLEFT_PAREN ( char ch ) {
	
	if ( ch == '(' )
		return true ;
	else 
		return false ;
	
} // IsLEFT_PAREN ()

bool IsRIGHT_PAREN ( char ch ) {
	
	if ( ch == ')' )
		return true ;
	else 
		return false ;
	
} // IsRIGHT_PAREN ()

bool IsDigit( char ch ) {
	
	if ( ch >= '0' && ch <= '9' )
		return true ;
	else
		return false ;
	
}  // IsDigit() 

bool IsQuote( char ch ) {
	
	if ( ch == '\'' )
		return true ;
	else 
		return false ;
		
} // IsQuote()

bool IsDoubleQuote( char ch ) {
	
	if ( ch == '\"' )
		return true ;
	else 
		return false ;
		
} // IsQuote()

bool IsLineComment ( char ch ) {
	
	if ( ch == ';' )
		return true ;
	else 
		return false ;
		
} // isLineComment

bool SeparatorEncounter ( char ch ) {
	
	if ( IsLEFT_PAREN ( ch ) || IsRIGHT_PAREN ( ch ) || IsQuote( ch ) || IsDoubleQuote( ch ) || IsWhiteSpace ( ch ) || IsLineComment ( ch ) )
		return true ;
	
	return false ;
	
}
// ---------------------------------------檢查字元end--------------------------------------------------- 

// ---------------------------------------skip start ( EOF Concern !!!)--------------------------------------- 
bool SkipThisLine() {
	
	if( cin.eof() )
		return false ;
		
	while ( gNextChar != '\n' ) { 
		if( !GetNextChar( gNextChar, gNextCharLine, gNextCharColumn ) ) 
			return false ; // 處理EOF
	} // while 
	
	if( !GetNextChar( gNextChar, gNextCharLine, gNextCharColumn ) ) 
		return false ; // 處理EOF
		
	return true ;
} // SkipThisLine()

bool SkipWhiteSpaces() {
	
	if( cin.eof() )
		return false ;
	
	while ( IsWhiteSpace ( gNextChar ) ) {
		if( !GetNextChar( gNextChar, gNextCharLine, gNextCharColumn ) ) 
			return false ; // 處理EOF
	} // while
	
	return true ; // 代表有跳過空格 
} // SkipWhiteSpaces()
// ---------------------------------------skip end------------------------------------------------------------



// ---------------------------------------Get 相關處理---------------------------------------------------- 
bool GetStringConst( Token & token ) {
	
	
	token.type = STRING ;
	token.line = gNextCharLine ;
	token.firstcolumn = gNextCharColumn ;
	token.tokenStr = token.tokenStr + gNextChar ;
	
	// 目前 token.tokenStr = "\"" 刪掉 \" 
	token.tokenStr = "" ;
	
	if( !GetNextChar( gNextChar, gNextCharLine, gNextCharColumn ) ) { 
		gNextCharColumn = gNextCharColumn + 1 ;// EOF也算一個字元
		return false ; // 處理EOF
	} 

		
	while( gNextChar != '\"' && gNextChar != '\n' ) {
		
		if ( gNextChar == '\\' ) {
			
			if( !GetNextChar( gNextChar, gNextCharLine, gNextCharColumn ) ) { 
				gNextCharColumn = gNextCharColumn + 1 ;// EOF也算一個字元
				return false ; // 處理EOF
			} 
				
			if( gNextChar == 'n' )
				token.tokenStr = token.tokenStr + '\n' ;
			else if ( gNextChar == 't' )
				token.tokenStr = token.tokenStr + '\t' ;
			else if ( gNextChar == '\"' )
				token.tokenStr = token.tokenStr + '\"' ;
			else if ( gNextChar == '\'' )
				token.tokenStr = token.tokenStr + '\'' ;
			else if ( gNextChar == '\\' )
				token.tokenStr = token.tokenStr + '\\' ;
			else {
				token.tokenStr = token.tokenStr + '\\' + gNextChar ;
			} // else
			
			if( !GetNextChar( gNextChar, gNextCharLine, gNextCharColumn ) )  {
				gNextCharColumn = gNextCharColumn + 1 ; // EOF也算一個字元 
				return false ; // 處理EOF
			}
				
				
		} // if
		else {
	        token.tokenStr = token.tokenStr + gNextChar ;
			if( !GetNextChar( gNextChar, gNextCharLine, gNextCharColumn ) ) {
				gNextCharColumn = gNextCharColumn + 1 ; // EOF也算一個字元 
				return false ; // 處理EOF
			}
				
		} // else

	} // while
	
	
	if ( gNextChar == '\"') {
		return true ;
	} // if
	else if ( gNextChar == '\n' ){ // EOF?? Line Column!!!!!!!!!!!!!!!
		return false ;
	} // else
	

	return true ;

} // GetStringConst()

bool GetTokenStr( Token & token ) {
	

	bool FirstChar = true ;
	
	while ( !SeparatorEncounter ( gNextChar ) && !cin.eof() ) {
		
		token.tokenStr = token.tokenStr + gNextChar ;
		
		if ( FirstChar ) {
			token.line = gNextCharLine ;
			token.firstcolumn = gNextCharColumn ;
			FirstChar = false ;
		} // if
		
		GetNextChar( gNextChar, gNextCharLine, gNextCharColumn ) ;
		
	} // while()
	

	return true ;
} // GetTokenStr()

// ---------------------------------------定義token type start------------------------------------------------

bool GetDOT( Token & token ) {
	
	
	if ( token.tokenStr == "." ) {
		token.type = DOT ;
		return true ;
	} // if
	
	return false ;
	
} // GetDOT

bool GetINT( Token & token ) {
	
	int FirstChar = 0 ;
	
	// 檢查是否為"+"或 "-" 這類單一符號，這些符號不屬於INT的範圍 
	if ( token.tokenStr == "+" || token.tokenStr == "-" )
		return false;
	
	
	for( int i = 0 ; i < token.tokenStr.size() ; i = i + 1 ) {
		
		if ( i == FirstChar ) {
			if ( token.tokenStr[i] != '+' && token.tokenStr[i] != '-' && !IsDigit( token.tokenStr[i] ) ) 
				return false ;	
		} // if
		else {
			if( !IsDigit( token.tokenStr[i] ) )
				return false ;
		} // else 
		
	} // for i
	
	token.type = INT ;
	
	try{
		token.Ivalue = stoi( token.tokenStr ) ;
	} catch (const std::invalid_argument& e) {
		while(true) {
			std::cerr << "錯誤: 輸入的字串無法轉換為整數！" << std::endl;
		}     	
    }
	return true ;
	
} // GetINT

bool GetFLOAT( Token & token ) {
	
	int FirstChar = 0 ;
	bool hasDot = false ;
	bool hasNum = false ;

	// 檢查是否為"+"或 "-" 或 "." 這類單一符號，這些符號不屬於FLOAT的範圍 
	if ( token.tokenStr == "+" || token.tokenStr == "-" || token.tokenStr == "." )
		return false;
	
	
	for( int i = 0 ; i < token.tokenStr.size() ; i = i + 1 ) {
		
		char ch = token.tokenStr[i] ;
		
		if ( i == FirstChar ) {
			
			if ( ch != '+' && ch != '-' && ch != '.' && !IsDigit( ch ) ) 
				return false ;	
				
			if ( ch == '.') 
                hasDot = true ;
            
			if ( IsDigit( ch ) )
				hasNum = true ;    
            
		} // if
		else {

			if ( IsDigit( ch ) )
				hasNum = true ;
				
			if( !IsDigit( ch ) && ch != '.' )
				return false ;
				
 			if (ch == '.') {
 				
                if ( hasDot == true ) 
                    return false;  // 如果已經有小數點，再遇到小數點返回 false
                    
                hasDot = true;  // 設置已經有小數點
                 
			} // if	
            
		} // else 
		
		
	} // for i
	
	if( !hasNum )
		return false ;
	
	token.type = FLOAT ;
	token.Fvalue = stod( token.tokenStr ) ;
	
	return true ;
	
} // GetFloat

bool GetNIL( Token & token ) {
	
	if ( token.tokenStr == "nil" || token.tokenStr == "#f" ) {
		token.tokenStr = "nil" ;
		token.type = NIL ;
		return true ;
	} // if
	
	return false ;
	
} // GetNIL

bool GetT( Token & token ) {

	if ( token.tokenStr == "t" || token.tokenStr == "#t" ) {
		token.tokenStr = "#t" ;
		token.type = T ;
		return true ;
	} // if
	
	return false ;

} // GetT

// Types to be defined: DOT INT FLOAT NIL T SYMBOL 
void DefineTokenType( Token & token ) {

	if ( GetDOT( token ) )
		return ;
	else if ( GetNIL(token) )
		return ;	
	else if ( 	GetT(token) )
		return ;
	else if ( GetINT( token ) )
		return ;
	else if ( GetFLOAT(token) )	
		return ;
	else
		token.type = SYMBOL ;
	
	return ;
} // DefineTokenType()

// ---------------------------------------定義token type end--------------------------------------------------------------

// ---------------------------------------Get Token (Lexer)---------------------------------------------------------------

void ClearToken( Token &token ) {
    token.tokenStr = "\0";
    token.type = DEFAULT; 
    token.line = -1;
    token.firstcolumn = -1;
}


// return true 代表已取完Token，return false 代表在取Token時遇到EOF(也代表Token後面緊接EOF)，保留Token，處理EOF 
bool GetToken( Token & token ) {
	
	ClearToken( token ) ;

	if ( SkipWhiteSpaces() == false ) {
		token.tokenStr = "EOF" ;
		token.type = EXIT ;
		return true;
	} // EOF 
	// separators '(' ')' '\'' ';' '\"'
	
	// Line-Comment
	while ( IsLineComment( gNextChar ) ) {
		
		if ( SkipThisLine() == false ) {
			token.tokenStr = "EOF" ;
			token.type = EXIT ;
			return true;
		} // EOF 
		
		if ( SkipWhiteSpaces() == false ) {
			token.tokenStr = "EOF" ;
			token.type = EXIT ;
			return true;
		} // EOF 
		
	} // if

	// 左括號 
	if ( IsLEFT_PAREN( gNextChar ) ) {
		token.tokenStr = token.tokenStr + '(' ;
		token.type = LEFT_PAREN ;
		token.line = gNextCharLine ;
		token.firstcolumn = gNextCharColumn ;
		GetNextChar( gNextChar, gNextCharLine, gNextCharColumn ) ;
	} // if ()
	
	// 右括號 
	else if ( IsRIGHT_PAREN( gNextChar ) ) {
		token.tokenStr = token.tokenStr + ')' ;
		token.type = RIGHT_PAREN ;	
		token.line = gNextCharLine ;
		token.firstcolumn = gNextCharColumn ;
		GetNextChar( gNextChar, gNextCharLine, gNextCharColumn ) ;
	} // else if()
	
	// 引號
	else if ( IsQuote( gNextChar ) ) {
		token.tokenStr = token.tokenStr + '\'' ;
		token.type = QUOTE ;
		token.line = gNextCharLine ;
		token.firstcolumn = gNextCharColumn ;
		GetNextChar( gNextChar, gNextCharLine, gNextCharColumn ) ;
	} // else if()
	
	// 雙引號 
	else if ( IsDoubleQuote ( gNextChar ) ) {
		
		if( !GetStringConst( token ) ) {
			token.tokenStr = "NoClosingQuote" ;
			token.type = ERROR ;
			token.line = gNextCharLine ;
			token.firstcolumn = gNextCharColumn ;

			return true ;
		} // if
		
		GetNextChar( gNextChar, gNextCharLine, gNextCharColumn ) ;
	} // else if() 	
	// else token
	else {
		GetTokenStr( token ) ;
		DefineTokenType( token ) ;
	} // else
	
	if (cin.eof()) {
		return false ;
	} // if
	
	return true ;
	
} // GetToken()
// ---------------------------------------Get Token (Lexer) end-----------------------------------------------------------





//----------------------------------------Parser------------------------------------------------------- 

// 新增lambda節點處理 ( 若是讀取到lambda節點直接輸出#<procedure lambda> 不用 (finish) (debug mode)
void printAST( const shared_ptr<ASTNode>& node, bool isDisplay, bool isWrite, int depth = 0, bool first = true, bool stay = false ) {
	
	if (!node) {
	    return;
	} // if
	
	

    if (node->type == ATOM) {
        shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(node);
		if (atom) {
		    string output = atom->value;
		    
		    if (atom->tokentype == STRING || atom->tokentype == ERROR) {
		    	if (!isDisplay)
		        	output = "\"" + output + "\"";
		    }	    
			
		    if (!stay)
		        cout << string(depth * 2, ' ') << output ;
		    else
		        cout << output ;
		    
		    if ( first ) {
		    	if ( !isDisplay && !isWrite ){
		    		cout << "\n" ;
				} // if
			} // if
			else{
				cout << "\n" ;
			}
		} // if
    } // if
    else if ( node->type == LAMBDA ) { // debug記得設為False // 回復原狀記得 
    	
    	shared_ptr<LambdaNode> L = static_pointer_cast<LambdaNode>(node) ;
    	string output = "#<procedure " + L->value + ">" ;
    	bool debug = false ;
    	
			if (!stay)
			    cout << string(depth * 2, ' ') << output ;
			else
			    cout << output ;

		    if ( first ) {
		    	if ( !isDisplay && !isWrite ){
		    		cout << "\n" ;
				} // if
			} // if
			else{
				cout << "\n" ;
			}
	} // else if
    else { // CONS
        shared_ptr<ConsNode> cons = static_pointer_cast<ConsNode>(node);
        
		if ( first ) 
        	cout << string(depth * 2, ' ') << "( ";
		
		// cons節點的left不會是nullptr 
		if ( cons->left->type == CONS ) { // left: CONS
		
			if(!stay)
		    	cout << string((depth + 1) * 2, ' ') << "( ";
			else 
				cout << "( ";  	
				
		    printAST(cons->left, isDisplay, isWrite, depth + 1, false, true); 
		    cout << string((depth + 1) * 2, ' ') << ")\n";
		    
		    if ( cons->right != nullptr ) {
				if ( cons->right->type == ATOM ) {
					shared_ptr<ATOMNode> atomR = static_pointer_cast<ATOMNode>(cons->right) ;
					if ( !(atomR->value == "nil" && atomR->tokentype == NIL) ) {
						cout << string((depth + 1) * 2, ' ') << ".\n" ;
						printAST (cons->right, isDisplay, isWrite, depth+1, false);
					} // if
				} // if
				else if( cons->right->type == CONS) {
					printAST(cons->right, isDisplay, isWrite, depth , false, false);
				}
				else if ( cons->right->type == LAMBDA ) {
					shared_ptr<LambdaNode> LambdaR = static_pointer_cast<LambdaNode>(cons->right) ;
						cout << string((depth + 1) * 2, ' ') << ".\n" ;
						printAST (cons->right, isDisplay, isWrite, depth+1, false);
				} // else if
			} // if
		    
		    
		} // if
		else if ( cons->left->type == ATOM ) {
			
			if ( cons->right != nullptr ) {
				if ( cons->right->type == ATOM ) { // left: ATOM right:ATOM
					
					shared_ptr<ATOMNode> atomL = static_pointer_cast<ATOMNode>(cons->left) ;
					shared_ptr<ATOMNode> atomR = static_pointer_cast<ATOMNode>(cons->right) ;
					
					if( atomR->value == "nil" && atomR->tokentype == NIL )   
						printAST( cons->left, isDisplay, isWrite, depth + 1, false, stay ); 
					else {
						printAST( cons->left, isDisplay, isWrite, depth + 1, false, stay ); 
						if ( !(atomR->value == "nil" && atomR->tokentype == NIL) ) {
							cout << string((depth + 1) * 2, ' ') << ".\n" ;
							printAST( cons->right, isDisplay, isWrite, depth + 1, false ); 	
						} // if
					} // else 

				} // if
				else if ( cons->right->type == LAMBDA ) { // left : ATOM  right : LAMBDA
					printAST( cons->left, isDisplay, isWrite, depth + 1, false, stay ); 
					cout << string((depth + 1) * 2, ' ') << ".\n" ;
					printAST( cons->right, isDisplay, isWrite, depth + 1, false ); 
				} // else if
				else { // left: ATOM right:CONS
					printAST( cons->left, isDisplay, isWrite, depth + 1, false, stay );
					printAST( cons->right, isDisplay, isWrite, depth , false ); 
				} // else 
			} // if
		} // else if
		else if ( cons->left->type == LAMBDA ) { // left : LAMBDA  right : ATOM // left : LAMBDA  right : CONS  // left : LAMBDA  right : LAMBDA 
			if ( cons->right != nullptr ) {
				if ( cons->right->type == ATOM ) {

					shared_ptr<ATOMNode> atomR = static_pointer_cast<ATOMNode>(cons->right) ;
					
					if( atomR->value == "nil" && atomR->tokentype == NIL )   
						printAST( cons->left, isDisplay, isWrite, depth + 1, false, stay );
					else {
						printAST( cons->left, isDisplay, isWrite, depth + 1, false, stay ); 
						cout << string((depth + 1) * 2, ' ') << ".\n" ;
						printAST( cons->right, isDisplay, isWrite, depth + 1, false ); 	
					} // else 
				} // if
				else if ( cons->right->type == LAMBDA ) {
						printAST( cons->left, isDisplay, isWrite, depth + 1, false, stay ); 
						cout << string((depth + 1) * 2, ' ') << ".\n" ;
						printAST( cons->right, isDisplay, isWrite, depth + 1, false ); 	
				} // else if
				else if ( cons->right->type == CONS ) {
					printAST( cons->left, isDisplay, isWrite, depth + 1, false, stay );
					printAST( cons->right, isDisplay, isWrite, depth , false ); 
				} // else if
				
			} // if
		} // else if

        if ( first ) {
        	
        	cout << string(depth * 2, ' ') << ")";
        	if ( !isDisplay && !isWrite )
        		cout << "\n" ;
        		
		}
        	
			
    } // else 
} // printAST () 

bool IsExit( const shared_ptr<ASTNode>& AST ) {
	
	if ( AST ) {
		if ( AST->type == CONS ) {  // 檢查是否為 CONS 節點
		    shared_ptr<ConsNode> cons = static_pointer_cast<ConsNode>(AST);
			if ( cons->left && cons->right && cons->left->type == ATOM && cons->right->type == ATOM ) { // 檢查 left 是否為 ATOMNode
				shared_ptr<ATOMNode> atomL = static_pointer_cast<ATOMNode>(cons->left) ;
				shared_ptr<ATOMNode> atomR = static_pointer_cast<ATOMNode>(cons->right) ;
		
			    if (atomL->value == "exit" && atomR->value == "nil" ) {
			    	return true;
			    } // if
			} // if
		} // if
	} // if
	return false ;
} // IsExit()


class Parser {
	
	private:
		Token currentToken ;
		bool Lookahead = false ;
		bool EofEncounter = false ;
		 

		void ResetLineAndColumn() {
		
			gLine = 1 ;    // the line-no of of the char that is yet to be read in
			gColumn = 2 ;  // the column-no of of the char that is yet to be read in
			gNextCharLine = 1 ;   // line-no of gNextChar (the char we just read in)
			gNextCharColumn = 1 ; // column-no of gNextChar (the char we just read in)
					
			if( gNextChar == '\n' ) {
				gLine = 2 ;    // the line-no of of the char that is yet to be read in
				gColumn = 1 ;  // the column-no of of the char that is yet to be read in
			} // if
		} // ResetLineAndColumn()
		
		bool SkipToNext() {
			if ( SkipWhiteSpaces() == false ) {
				EofEncounter = true ;
				currentToken.tokenStr = "EOF" ;
				currentToken.type = EXIT ;
				return false ;
			} // EOF 
			// separators '(' ')' '\'' ';' '\"'
			
			// Line-Comment
			while ( IsLineComment( gNextChar ) ) {
				
				if ( SkipThisLine() == false ) {
					EofEncounter = true ;
					currentToken.tokenStr = "EOF" ;
					currentToken.type = EXIT ;
					return false ;
				} // EOF 
				
				if ( SkipWhiteSpaces() == false ) {
					EofEncounter = true ;
					currentToken.tokenStr = "EOF" ;
					currentToken.type = EXIT ;
					return false ;
				} // EOF 
				
			} // while
    		if ( gNextCharLine > 1 ) { 
    			gLine = gLine - 1 ;
    			gNextCharLine = gNextCharLine - 1 ;
    		}  // if
    		
    		return true ;
		} // SkipToNext
		
		bool IsValidType( TokenType type ) {
			if ( type == INT || type == STRING || type == FLOAT || type == NIL || type == T || type == SYMBOL ||
				 type == LEFT_PAREN || type == QUOTE || type == EXIT || type == ERROR )
				return true ;
			
			return false ;
		} // IsValidType()
		
		void NextToken() {
			
			if(!Lookahead){
				
				if( !GetToken( currentToken ) ) {
					EofEncounter = true ;
				} // if
//				PrintToken(currentToken) ;
				Lookahead = true ;
			}
			
		} // NextToken
		
		void ConsumeToken() {
			
			if (!Lookahead) {
				throw runtime_error("No lookahead token to consume");
			} // if
			else
				Lookahead = false ;
		
		} // ConsumeToken
		

		
		shared_ptr<ASTNode> parseSExp() {
			
			shared_ptr<ASTNode> result ;
			NextToken() ; 
			
			
			// ATOM 
			if ( IsValidType( currentToken.type ) ){
				
				if (currentToken.type == EXIT ){
					throw NoMoreInputException() ;
				} // if
				
				else if ( IsATOMinNode( currentToken.type ) ) {
					
					
					string str = currentToken.tokenStr ;
					
					if ( ( currentToken.tokenStr == "nil" && currentToken.type == NIL ) || ( currentToken.tokenStr == "#f" && currentToken.type == NIL ) ) {
						str = "nil" ;
		         	} //if 
					else if ( currentToken.type == FLOAT ) {
						stringstream ss;
						ss << fixed << setprecision(3) << currentToken.Fvalue;
						str = ss.str() ;
//						str = currentToken.tokenStr ;
					} // if
					else if ( currentToken.type == INT ) {	
						str = to_string( currentToken.Ivalue ) ;
					} // if
					
		            result = make_shared<ATOMNode>(str,currentToken.type);
		            	
		            ConsumeToken() ; 
				} // else if
				// S-Exp 
				else if ( currentToken.type == LEFT_PAREN ) {
					ConsumeToken() ;
					result = parseList() ;
				} // else if
				// QUOTE
				else if ( currentToken.type == QUOTE ) {
					ConsumeToken() ;
					result = make_shared<ConsNode>( make_shared<ATOMNode>( "quote", QUOTE ), make_shared<ConsNode>( parseSExp(), make_shared<ATOMNode>( "nil", NIL) ) ) ; // nullptr
				} // else if
	
				else if ( currentToken.type == ERROR ){
					if ( currentToken.tokenStr == "NoClosingQuote" ){
						int Line = gNextCharLine ;
						int Colunm = gNextCharColumn ;
						
						if ( !SkipThisLine() ) {
							currentToken.tokenStr = "EOF" ;
							currentToken.type = EXIT ;
							currentToken.line = gNextCharLine ;
							currentToken.firstcolumn = gNextCharColumn ;
							EOFExit = true ;
						}
						else 
							ConsumeToken() ;
							
						ResetLineAndColumn() ;
						throw NoClosingQuoteException( Line, Colunm ) ;
					} // if
				} // else if
	
				return result ;
			}
			else {
				int line = currentToken.line ; ;
				int column = currentToken.firstcolumn ;
				string str = currentToken.tokenStr ;
	
				if ( !SkipThisLine() ) {
					currentToken.tokenStr = "EOF" ;
					currentToken.type = EXIT ;
					currentToken.line = gNextCharLine ;
					currentToken.firstcolumn = gNextCharColumn ;
				}
				else 
					ConsumeToken() ;
				
	
				ResetLineAndColumn() ;
				throw UnExpectedTokenException( line, column, str ) ;
			}
			
		} // parseExpresstion()
		
		shared_ptr<ASTNode> parseList() {
			
			// LEFT-PAREN <S-exp> { <S-exp> } [ DOT <S-exp> ] RIGHT-PAREN 
			NextToken() ;
	
			// RIGHT-PAREN 
			if ( currentToken.type == RIGHT_PAREN ) {
				ConsumeToken() ; 
				return make_shared<ATOMNode>("nil", NIL) ;
			} // if
			
			// <S-exp>
			shared_ptr<ASTNode> left = parseSExp() ;
			
			NextToken() ;
	
			// [ DOT <S-exp> ]
			if ( currentToken.type == DOT ) {
				
				ConsumeToken() ;
				// <S-exp>
				shared_ptr<ASTNode> right = parseSExp() ; 
				NextToken() ; 
	
				if ( currentToken.type != RIGHT_PAREN ) {
					int line = currentToken.line ; 
					int column = currentToken.firstcolumn ;
					string str = currentToken.tokenStr ;
					
					if ( !SkipThisLine() ) {
						currentToken.tokenStr = "EOF" ;
						currentToken.type = EXIT ;
					}
					else 
						ConsumeToken() ;
				
					ResetLineAndColumn() ;
					
					throw MissingRightParenException( line, column, str ) ;
				}
				
				ConsumeToken() ;
				return make_shared<ConsNode>( left, right ) ;
				
			} // DOT
	
			//  { <S-exp> } 
			shared_ptr<ASTNode> right = parseList() ;
			return make_shared<ConsNode>( left, right ) ;
				
		} // parseList
		

	public:
		
		Parser(){}
		 
    	shared_ptr<ASTNode> parse() {
    		

    		if( EofEncounter ) {
    			shared_ptr<ASTNode> result = nullptr ;
    			if( Lookahead ) {
    				result = parseSExp() ;
				}
				
    			throw NoMoreInputException(result) ;
			} // if
    			
    		shared_ptr<ASTNode> result = parseSExp() ;

			// 在這裡重製行列 	
    		ResetLineAndColumn() ; 
    		
    		if ( SkipToNext() == false )
    			return result ;
			 
			NextToken() ;
			 
        	return result ;
		} // parse
		
		
}; // class parser

Parser g_parser ; // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

class Evaluator {
	
	private:
		map< string, shared_ptr<ASTNode> > symbolTable;
		bool dontCareBool = false ;
		bool ignore = false ;
		bool Verbose = true ;
		
		shared_ptr<ASTNode> CloneNode( shared_ptr<ASTNode> node ) {
			
			if (node == nullptr) 
				return nullptr;
	
			if( node->type == ATOM ) {
				shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(node);
				shared_ptr<ATOMNode> cloneATOM = make_shared<ATOMNode>(atom->value, atom->tokentype) ;
				return cloneATOM ;
			} // if
			else if ( node->type == CONS ) {
				shared_ptr<ConsNode> cons = static_pointer_cast<ConsNode>(node);
				shared_ptr<ConsNode> cloneCONS = make_shared<ConsNode>( CloneNode( cons->left ), CloneNode(cons->right) ) ;
				return cloneCONS ;
			} // else if
			else if ( node->type == LAMBDA ) {
				shared_ptr<LambdaNode> lambda = static_pointer_cast<LambdaNode>(node) ;
				shared_ptr<LambdaNode> cloneLAMBDA = make_shared<LambdaNode>( CloneNode( lambda->variables ) , CloneNode( lambda->functions ), lambda->value ) ;
				return cloneLAMBDA ; 
			}
		    else {
		        throw std::runtime_error("CloneNode: Unsupported node type");
		    }
		} // CloneNode()
		
		bool IsString( shared_ptr<ATOMNode> atom ) {	
			return ( atom->tokentype == STRING ) ;
		} // IsString()
		
		bool IsINT( shared_ptr<ATOMNode> atom ) {
			return ( atom->tokentype == INT ) ;
		} // isINT
		
		bool IsFLOAT( shared_ptr<ATOMNode> atom ) {
			return ( atom->tokentype == FLOAT ) ;
		} // IsFLOAT()
		
		bool IsBooleanTrue( shared_ptr<ATOMNode> atom ) {
				return atom->tokentype == T ;
		} // IsBooleanTrue()
		
		bool IsBoolean( shared_ptr<ATOMNode> atom ) {
			return ( atom->tokentype == NIL || atom->tokentype == T ) ;
		} // IsBoolean()
		
		//nil 是空 list，也代表 false（對應 #f）
		bool IsNIL ( shared_ptr<ATOMNode> atom ) {
			return atom->tokentype == NIL ;
		} // IsNIL()
		
		bool IsLiteral( shared_ptr<ATOMNode> atom ) {
			return ( IsINT(atom) || IsFLOAT(atom) || IsBooleanTrue(atom) || IsNIL(atom) || IsString(atom) ) ;
		} // IsLiteral()
		
		// local variable (已完成)
		bool IsBoundSymbol( shared_ptr<ATOMNode> atom, map< string, shared_ptr<ASTNode> > & localSymbolTable ) {
		
			if ( IsLiteral(atom) ) {
				return false ;
			} // if
			
			if ( localSymbolTable.count(atom->value) > 0 ) {
				return true ;
			} // if
			
			if ( symbolTable.count( atom->value ) > 0 ) {
				return true ;
			} // if	

			return false ; 

		} // IsBoundSymbol
		
		bool IsFunctionName( string name ) { // "\'" 不再此但在EvalCons依舊算function
			return ( name == "quote" || name == "define" || name == "if" || name == "cond" || name == "begin" || IsBuiltIn(name) || name == "clean-environment" || name == "exit" || name == "lambda" ||
					 name == "let" || name == "verbose" || name == "verbose?" || name == "create-error-object" || name == "error-object?" || name == "read" || name == "write" || name == "display-string" ||
					 name == "newline" || name == "symbol->string" || name == "number->string" || name == "eval" || name == "set!" )  ;
		} // IsFunctionName()
		
		bool IsBuiltIn( string name ) {
		    // connect
		    if (name == "cons" || name == "list")
		        return true;
		
		    // car cdr
		    if (name == "car" || name == "cdr")
		        return true;
		
		    // Predicates
		    if (name == "atom?" || name == "pair?" || name == "list?" || name == "null?" ||
		        name == "integer?" || name == "real?" || name == "number?" ||
		        name == "string?" || name == "boolean?" || name == "symbol?")
		        return true;
		
		    // Arithmetic
		    if (name == "+" || name == "-" || name == "*" || name == "/")
		        return true;
		
		    // Logic
		    if (name == "not" || name == "and" || name == "or")
		        return true;
		
		    // Comparison
		    if (name == ">" || name == ">=" || name == "<" || name == "<=" || name == "=")
		        return true;
		
		    // String
		    if (name == "string-append" || name == "string>?" || name == "string<?" || name == "string=?")
		        return true;
		
		    // Equal
		    if (name == "eqv?" || name == "equal?")
		        return true;

		    return false;
		}
		
		bool IsBuiltInFunction( string name ) {
		    // connect
		    if (name == "#<procedure cons>" || name == "#<procedure list>")
		        return true;
		
		    // car cdr
		    if (name == "#<procedure car>" || name == "#<procedure cdr>")
		        return true;
		
		    // Predicates
		    if (name == "#<procedure atom?>" || name == "#<procedure pair?>" || name == "#<procedure list?>" || name == "#<procedure null?>" ||
		        name == "#<procedure integer?>" || name == "#<procedure real?>" || name == "#<procedure number?>" ||
		        name == "#<procedure string?>" || name == "#<procedure boolean?>" || name == "#<procedure symbol?>")
		        return true;
		
		    // Arithmetic
		    if (name == "#<procedure +>" || name == "#<procedure ->" || name == "#<procedure *>" || name == "#<procedure />")
		        return true;
		
		    // Logic
		    if (name == "#<procedure not>" || name == "#<procedure and>" || name == "#<procedure or>")
		        return true;
		
		    // Comparison
		    if (name == "#<procedure >>" || name == "#<procedure >=>" || name == "#<procedure <>" || name == "#<procedure <=>" || name == "#<procedure =>")
		        return true;
		
		    // String
		    if (name == "#<procedure string-append>" || name == "#<procedure string>?>" || name == "#<procedure string<?>" || name == "#<procedure string=?>")
		        return true;
		
		    // Equal
		    if (name == "#<procedure eqv?>" || name == "#<procedure equal?>")
		        return true;

		    return false;
		}

		shared_ptr<ASTNode> EvalQuote( shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode ) {
			
			vector<shared_ptr<ASTNode>> results = GetRawParameters( node, "quote", funcNode ) ;
			
			if ( results.size() != 1 ) {
				throw WrongNumberOfArgumentException("quote") ;
			}
			
			return CloneNode(results[0]) ;
			
		} // EvalQuote
		

		// 若是 function、if、cond、and、or → 負責轉成專屬錯誤訊息（會 throw 新的錯誤）
		// 若是 define / set! / let → 不會轉錯，但會設定 hasBeenClassified 為 true，讓錯誤型別保持為 NoReturnValue
		// 若其他包裹結構（如 begin）接收到時，若錯誤已分類 → 不轉換，直接 throw 回 top-level		
		// no return error done! 
	    shared_ptr<ASTNode> EvalDefine(shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable) {
	      
			vector<shared_ptr<ASTNode>> results = GetRawParameters( node, "define", funcNode );
	
	    	if ( results.size() < 2 )
	    		throw DefineFormatException();
	    	else if ( results.size() > 2 )
	      		return EvalDefine2( node, funcNode, localSymbolTable ) ;
	
	      	shared_ptr<ASTNode> variable = results[0];
	      	shared_ptr<ASTNode> value = nullptr;
	
	      	if (variable->type != ATOM)
	        	return EvalDefine2( node, funcNode, localSymbolTable ) ;
	
	      	shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(variable);
	      	string varName = atom->value;
	
	      	if (IsLiteral(atom) || IsFunctionName(varName))
	        	throw DefineFormatException();
	
	      // handle (define b a)  b 與 a 指向同一份物件
	      	shared_ptr<ATOMNode> tempAtom = nullptr;
	      	if (results[1]->type == ATOM)
	        	tempAtom = static_pointer_cast<ATOMNode>( results[1] );
	
	      	if (tempAtom && IsBoundSymbol(tempAtom, localSymbolTable)) {
	        	symbolTable[varName] = GetRawVariable(tempAtom->value, localSymbolTable);  // no clone
	      	} else {
	      		try{
	      			value = Eval( results[1], dontCareBool, ignore, false, localSymbolTable );
				} catch ( NoReturnValueException & e ) {
					throw NoReturnValueException( e.GetNode(), true );			
				} // catch

	        	symbolTable[varName] = CloneNode(value);  // clone for safety
	      	}
			
			cout << varName << " defined\n" ;
//	      	return make_shared<ATOMNode>( varName, SYMBOL );
	      	return nullptr ;
	    } // EvalDefine
	    
	    // > 2 個參數 // 變數不是atom 
		// syntactic sugar
	    shared_ptr<ASTNode> EvalDefine2(shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable) {
	    	

	    	shared_ptr<ConsNode> cons = static_pointer_cast<ConsNode>(node) ;
	    	shared_ptr<ASTNode> NameAndVariables = CloneNode( cons->left ) ;
	    	shared_ptr<ASTNode> Functions = CloneNode( cons->right ) ;
	    	
	    	// 取出name and variable 的同時，檢查參數集合中的s-exp是否不是non-list error 
	    	vector<shared_ptr<ASTNode>> results = GetRawParameters( NameAndVariables, "define2", funcNode ) ;
	    		
	    	for ( auto n : results ) { // 檢查function name 和 variable name 若不是atom，即丟出錯誤 
	    		if ( n->type != ATOM )
	    			throw DefineFormatException() ;
			} // for
				
			shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(results[0]) ;
		    string funcName = atom->value  ;
		      	
			if (IsLiteral(atom) || IsFunctionName(funcName)) // 檢查Function name 是否為保留字或著數值 
		        throw DefineFormatException();
		        
		    shared_ptr<ConsNode> cons2 = static_pointer_cast<ConsNode>(NameAndVariables) ; // 有經過 GetRawParameters檢驗過 NameAndVariables是一個cons 
		    shared_ptr<ASTNode> Variables = CloneNode( cons2->right ) ; 
		    
		    if ( Functions->type == ATOM ) {
			    shared_ptr<ATOMNode> atomF = static_pointer_cast<ATOMNode>(Functions);
			    if (IsNIL(atomF)) {
			        throw DefineFormatException(); // 無 body
			    } // if
			} // if
			
	    	shared_ptr<LambdaNode> lambda = make_shared<LambdaNode>( Variables, Functions, funcName ) ;
	    	symbolTable[funcName] = lambda ;
			
			if (Verbose)
				cout << funcName << " defined\n" ;
//			return make_shared<ATOMNode>( funcName, SYMBOL ) ; 
	    	return nullptr ;
		} // EvalDefine2() 

		// no return error done!
		shared_ptr<ASTNode> EvalIF( shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable ) {


			vector< shared_ptr<ASTNode> > results = GetRawParameters( node, "if", funcNode ) ;
			
			
			// 參數錯誤 
			if ( results.size() != 2 && results.size() != 3 ) {
				throw WrongNumberOfArgumentException("if") ;
			} // if
			
			shared_ptr<ASTNode> cond = nullptr ;
			try {
				cond = Eval( results[0], dontCareBool, ignore, false, localSymbolTable) ;
			} catch ( NoReturnValueException & e ) {
				if (!e.GetClassified())
					throw UnboundTestConditionException( e.GetNode() ) ;
				else 
					throw ;					
			} // catch
			bool isFalse = false ;
				
			// 判斷cond是否為false 
			if ( cond->type == ATOM ) {
				shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(cond);
					
				if ( IsNIL ( atom ) ) {
					isFalse = true ;
				} // if
							
			} // if

			
			// 處理兩個參數的情況 
			if ( results.size() == 2 ) {
				
				if ( isFalse ) {
					throw NoReturnValueException(funcNode) ;
				} // if
				
				shared_ptr<ASTNode> ans = nullptr ;
				try {
					ans = Eval(results[1], dontCareBool, ignore, false, localSymbolTable) ;
				} catch ( NoReturnValueException& e ) {
					if ( !e.GetClassified() )
						throw NoReturnValueException(funcNode) ;
					else
						throw ;
				}

				return ans ;
					
			} // if
			
			// 處理三個參數的情況 
			else if ( results.size() == 3 ) {
				shared_ptr<ASTNode> ans = nullptr ;
				 
				if ( isFalse ) {
					ans = Eval(results[2], dontCareBool, ignore, false, localSymbolTable) ;
					return ans ;
				} // if
				
				ans = Eval(results[1], dontCareBool, ignore, false, localSymbolTable) ;
				return ans ;
			}
			throw runtime_error("ERROR in EvalIF") ;
		} // EvalIF
		
		// no return error done!
		shared_ptr<ASTNode> EvalCond( shared_ptr<ASTNode> node , shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable ) {
		    vector< shared_ptr<ASTNode> > exprs = GetRawParameters(node, "cond", funcNode);
		
		    if ( exprs.size() < 1 ) {
		        throw CondFormatException(funcNode);
		    }
			
			// 先檢查各個判斷式的格式 
			for ( int i = 0 ; i < exprs.size() ; i++ ) {
		        vector< shared_ptr<ASTNode> > elements = GetCondAndPos(exprs[i], funcNode);
		        
		        if ( elements.size() < 2) {
		            throw CondFormatException(funcNode);
		        } // if
			} // for
			
		    for (int i = 0; i < exprs.size(); i++) {
		    	
		        vector< shared_ptr<ASTNode> > elements = GetCondAndPos(exprs[i], funcNode);

		        
				bool isFalse = false;
		        bool matched = false;
		        
		        // -----------------------------------cond處理 
		
		        if (elements[0]->type == ATOM) {
		            shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(elements[0]);
		
		            // 合法 else 分支：必須是最後一個
		            if (atom->value == "else" && i == exprs.size() - 1) {
			            shared_ptr<ASTNode> result = nullptr;
			            for (int j = 1; j < elements.size(); j++) {
			            	
			            	shared_ptr<ASTNode> temp = nullptr ;
			            	try {
			            		temp = Eval(elements[j], dontCareBool, ignore, false, localSymbolTable);
							} catch ( NoReturnValueException& e ) {
								if ( !e.GetClassified() ) {
									if ( j == elements.size() - 1)
										throw NoReturnValueException(funcNode) ;
									else 
										continue ;
								} // if
								else {
									throw ;
								} // else
	
							} // catch
			                
			                if (j == elements.size() - 1)
			                    result = temp; // 保留最後一個
			            } // for
			            return result;
		            }
		        } // if
		        

				shared_ptr<ASTNode> cond = nullptr ; 
				try {
					cond = Eval(elements[0], dontCareBool, ignore, false, localSymbolTable); 
				} catch ( NoReturnValueException& e ) {
					if ( !e.GetClassified() )
						throw UnboundTestConditionException( e.GetNode() ) ;
					else {
						throw ;
					}
				} // catch
				
		        if (cond->type == ATOM) {
		            shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(cond);
		            if ( IsNIL ( atom ) ) {
		                isFalse = true;
		            }
		        }
		        
		        // -----------------------------------cond處理 
		        
		        if (!isFalse) {
		            // 執行對應的表達式們，回傳最後一個
		            shared_ptr<ASTNode> result = nullptr;
		            for (int j = 1; j < elements.size(); j++) {
		            	
		            	shared_ptr<ASTNode> temp = nullptr ;
		            	try {
		            		temp = Eval(elements[j], dontCareBool, ignore, false, localSymbolTable);
						} catch ( NoReturnValueException& e ) {
							if ( !e.GetClassified() ) {
								if ( j == elements.size() - 1)
									throw NoReturnValueException(funcNode) ;
								else 
									continue ;
							} // if
							else {
								throw ;
							} // else

						} // catch
		                
		                
		                
		                if (j == elements.size() - 1)
		                    result = temp; // 保留最後一個
		            } // for
		            return result;
		        } // if

		    } // for i
		
		    throw NoReturnValueException(funcNode);
		
		} // EvalCond()
		
	    shared_ptr<ASTNode> EvalCleanEnvironment(shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode) {
	      vector<shared_ptr<ASTNode>> parameters = GetRawParameters(node, "clean-environment", funcNode);
	
	      if (!parameters.empty()) {
	        throw WrongNumberOfArgumentException("clean-environment");
	      }
	
	      symbolTable.clear();
	      if (Verbose) {
	    	cout << "environment cleaned\n" ;
		  }
//	      return make_shared<ATOMNode>("environment cleaned", SYMBOL);
		  return nullptr ;
	    }
		

		shared_ptr<ASTNode> EvalBegin( shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable ) {
			
			vector<shared_ptr<ASTNode>> parameters = GetRawParameters( node, "begin", funcNode ) ;
			
			if ( parameters.size() < 1 ) {
				throw WrongNumberOfArgumentException( "begin" ) ;
			}
			
			vector< shared_ptr<ASTNode> > results ;
			
			for ( int i = 0 ; i < parameters.size() ; i = i + 1 ) {
				try {
					results.push_back( Eval(parameters[i], dontCareBool, ignore, false, localSymbolTable ) ) ; 
				} catch ( NoReturnValueException& e ) {
					if ( i != parameters.size() - 1 ) {
						continue ;
					} // if
					else {
						if ( !e.GetClassified() )
							throw NoReturnValueException(funcNode) ;
						else
							throw ;
					} // else 
				}
				
			} // for
			
			shared_ptr<ASTNode> ans = results.back() ; // 保留最後要return的result 
			
			return ans ;
			
		} // EvalBegin

		shared_ptr<ASTNode> EvalCreateCons( shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable ) {
			
			vector<shared_ptr<ASTNode>> parameters = GetRawParameters( node, "cons", funcNode ) ;
			
			if ( parameters.size() != 2 ) {
				throw WrongNumberOfArgumentException( "cons" ) ;
			}
			
			vector< shared_ptr<ASTNode> > results = EvalAllParameters(parameters, funcNode, localSymbolTable);

			
			shared_ptr<ConsNode> cons = make_shared<ConsNode>( CloneNode( results[0] ), CloneNode( results[1] ) ) ;
			return cons;
			
		} // EvalCreateCons()
		
		shared_ptr<ASTNode> EvalList( shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable )  {
			
			vector< shared_ptr<ASTNode> > parameters = GetRawParameters( node, "list", funcNode ) ;
			
			if ( parameters.empty() ) {
				return make_shared<ATOMNode>("nil", NIL) ;
			}
			
			vector< shared_ptr<ASTNode> > results = EvalAllParameters(parameters, funcNode, localSymbolTable);

			
			shared_ptr<ConsNode> head = make_shared<ConsNode>( results[0] ) ;
			shared_ptr<ConsNode> current = head ;
			
			for ( int i = 1 ; i < results.size() ; i = i + 1 ) {
				
				current->right = make_shared<ConsNode>( results[i] ) ;
				current = static_pointer_cast<ConsNode>(current->right);
				
			} // for i
			
			current->right = make_shared<ATOMNode>("nil", NIL) ;
			
			return head ;
			
		} // EvalList ()
		
		shared_ptr<ASTNode> EvalCarCdr( shared_ptr<ASTNode> node, string name, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable ) {
			
			vector<shared_ptr<ASTNode>> parameters = GetRawParameters( node, name, funcNode ) ;
			
		    if (parameters.size() != 1) {
		    	throw WrongNumberOfArgumentException(name);
			}
			
			if ( parameters[0]->type == ATOM ) {
				shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(parameters[0]) ;
				
				if ( IsBoundSymbol(atom, localSymbolTable) ) {
					shared_ptr<ASTNode> Value = GetRawVariable( atom->value, localSymbolTable ) ;
					
					if ( Value->type != CONS ) { 
						throw IncorrectArgumentTypeException( CloneNode(Value), name) ;
					} // if
					else if( Value->type == CONS ) {
						shared_ptr<ConsNode> cons = static_pointer_cast<ConsNode>(Value) ;
						

						if ( name == "car")
		        			return cons->left ;
		        		else if ( name == "cdr" )
		        			return cons->right ;
		        		else 
		        			throw runtime_error("Unknown function name in EvalCarCdr 1: " + name);
					} // else if
					
				} // if
				else {
					if ( atom->tokentype == SYMBOL )
						throw UnboundSymbolException(atom->value) ;
					else
						throw IncorrectArgumentTypeException( parameters[0] , name ) ;
				}
			} // if
			
			
			
			else if ( parameters[0]->type == CONS ) {
				

				vector< shared_ptr<ASTNode> > results = EvalAllParameters(parameters, funcNode, localSymbolTable);
				
			    if (results[0]->type == CONS) {
			        shared_ptr<ConsNode> cons = static_pointer_cast<ConsNode>(results[0]);
			        
					if ( name == "car")
			        	return cons->left ;
			        else if ( name == "cdr" )
			        	return cons->right ;
			        else {
			        	throw runtime_error("Unknown function name in EvalCarCdr 2: " + name);
					}

			    } // if
			    else if ( results[0]->type == ATOM ) {
					throw IncorrectArgumentTypeException( CloneNode( results[0] ) , name) ;
			    } // else if
			    else if ( results[0]->type == LAMBDA ) {
			    	throw IncorrectArgumentTypeException( CloneNode( results[0] ) , name) ;
				} // else if
			    // 保底處理
			    else { 
			    	throw runtime_error("Unknown function name in EvalCarCdr 3: " + name);
				} 
			} // else if
		
				throw runtime_error("ERROR in EvalCarCdr") ;
		} // EvalCarCdr

	    shared_ptr<ASTNode> EvalIsAtom( shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable ) {
	      
		  vector<shared_ptr<ASTNode>> parameters = GetRawParameters(node, "atom?", funcNode);
	
	      if (parameters.size() != 1) {
	        throw WrongNumberOfArgumentException("atom?");
	      }
	
	      vector<shared_ptr<ASTNode>> results = EvalAllParameters(parameters, funcNode, localSymbolTable);
	
	      if (results[0]->type == ATOM) 
	        return make_shared<ATOMNode>("#t", T);
	      else 
	        return make_shared<ATOMNode>("nil", NIL);

	      
	    } // EvalIsAtom
		
	    shared_ptr<ASTNode> EvalIsPair(shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable) {
	      vector<shared_ptr<ASTNode>> parameters = GetRawParameters(node, "pair?", funcNode);
	
	      if (parameters.size() != 1) {
	        throw WrongNumberOfArgumentException("pair?");
	      }
	
	      vector<shared_ptr<ASTNode>> results = EvalAllParameters(parameters, funcNode, localSymbolTable);
	
	      if (results[0]->type == CONS) {
	        return make_shared<ATOMNode>("#t", T);
	      }
		  else 
	        return make_shared<ATOMNode>("nil", NIL);
	      

	    } // EvalIsPair()
		
	    shared_ptr<ASTNode> EvalIsList(shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable) {
	      vector<shared_ptr<ASTNode>> parameters = GetRawParameters(node, "list?", funcNode);
	
	      if (parameters.size() != 1) {
	        throw WrongNumberOfArgumentException("list?");
	      }
	
	      vector<shared_ptr<ASTNode>> results = EvalAllParameters(parameters, funcNode, localSymbolTable);
	
	      if (IsCorrectList(results[0])) {
	        return make_shared<ATOMNode>("#t", T);
	      } else {
	        return make_shared<ATOMNode>("nil", NIL);
	      }
	    } // EvalIsList()
	    	
		shared_ptr<ASTNode> EvalIsNull(shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable) {
		    vector<shared_ptr<ASTNode>> parameters = GetRawParameters(node, "null?", funcNode);
		
		    if (parameters.size() != 1) {
		        throw WrongNumberOfArgumentException("null?");
		    }
		
		    vector<shared_ptr<ASTNode>> results = EvalAllParameters(parameters, funcNode, localSymbolTable);
		
		    if (results[0]->type == ATOM) {
		        shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(results[0]);
		        if (IsNIL(atom)) {
		            return make_shared<ATOMNode>("#t", T);
		        }
		    }
		
		    return make_shared<ATOMNode>("nil", NIL);
		} // EvalIsNull()
		
		shared_ptr<ASTNode> EvalIsInteger(shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable) {
		    vector<shared_ptr<ASTNode>> parameters = GetRawParameters(node, "integer?", funcNode);
		
		    if (parameters.size() != 1) {
		        throw WrongNumberOfArgumentException("integer?");
		    }
		
		    vector<shared_ptr<ASTNode>> results = EvalAllParameters(parameters, funcNode, localSymbolTable);
		
		    if (results[0]->type == ATOM) {
		        shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(results[0]);
		        if (IsINT(atom)) {
		            return make_shared<ATOMNode>("#t", T);
		        }
		    }
		
		    return make_shared<ATOMNode>("nil", NIL);
		} // EvalIsInteger()
		
		shared_ptr<ASTNode> EvalIsRealAndNumber(shared_ptr<ASTNode> node, string name, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable) {
		    vector<shared_ptr<ASTNode>> parameters = GetRawParameters(node, name, funcNode);
		
		    if (parameters.size() != 1) {
		        throw WrongNumberOfArgumentException(name);
		    }
		
		    vector<shared_ptr<ASTNode>> results = EvalAllParameters(parameters, funcNode, localSymbolTable);
		
		    if (results[0]->type == ATOM) {
		        shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(results[0]);
		        if (IsINT(atom) || IsFLOAT(atom)) {
		            return make_shared<ATOMNode>("#t", T);
		        }
		    }
		
		    return make_shared<ATOMNode>("nil", NIL);
		} // EvalIsRealAndNumber
		
		shared_ptr<ASTNode> EvalIsString(shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable) {
		    vector<shared_ptr<ASTNode>> parameters = GetRawParameters(node, "string?", funcNode);
		
		    if (parameters.size() != 1) {
		        throw WrongNumberOfArgumentException("string?");
		    }
		
		    vector<shared_ptr<ASTNode>> results = EvalAllParameters(parameters, funcNode, localSymbolTable);
		
		    if (results[0]->type == ATOM) {
		        shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(results[0]);
		        if (IsString(atom)) {
		            return make_shared<ATOMNode>("#t", T);
		        }
		    }
		
		    return make_shared<ATOMNode>("nil", NIL);
		} // EvalIsRealAndNumber()
		
		shared_ptr<ASTNode> EvalIsBoolean(shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable ) {
		    vector<shared_ptr<ASTNode>> parameters = GetRawParameters(node, "boolean?", funcNode);
		
		    if (parameters.size() != 1) {
		        throw WrongNumberOfArgumentException("boolean?");
		    }
		
		    vector<shared_ptr<ASTNode>> results = EvalAllParameters(parameters, funcNode, localSymbolTable);
		
		    if (results[0]->type == ATOM) {
		        shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(results[0]);
		        if (IsBoolean(atom)) {
		            return make_shared<ATOMNode>("#t", T);
		        }
		    }
		
		    return make_shared<ATOMNode>("nil", NIL);
		} // EvalIsBoolean
		
		shared_ptr<ASTNode> EvalIsSymbol(shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable) {
		    vector<shared_ptr<ASTNode>> parameters = GetRawParameters(node, "symbol?", funcNode);
		
		    if (parameters.size() != 1) {
		        throw WrongNumberOfArgumentException("symbol?");
		    }
		
		    vector<shared_ptr<ASTNode>> results = EvalAllParameters(parameters, funcNode, localSymbolTable);
		
		    if (results[0]->type == ATOM) {
		        shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(results[0]);
		        if (!IsLiteral(atom)) {
		            return make_shared<ATOMNode>("#t", T);
		        }
		    }
		
		    return make_shared<ATOMNode>("nil", NIL);
		} // EvalIsSymbol
		
	    shared_ptr<ASTNode> Calculator(shared_ptr<ASTNode> node, string name, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable) {
	    	vector<shared_ptr<ASTNode>> parameters = GetRawParameters(node, name, funcNode);
	    	bool hasFloat = false;
	    	double ans = 0;
	
	    	if (parameters.size() < 2) {
	    		throw WrongNumberOfArgumentException(name);
	    	}
	
	    	vector<shared_ptr<ASTNode>> results = EvalAllParameters(parameters, funcNode, localSymbolTable);
	
	    	for (int i = 0; i < results.size(); ++i) {
		    	if (results[i]->type != ATOM) {
		        	throw IncorrectArgumentTypeException(CloneNode(results[i]), name);
		    	}
		
		    	shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(results[i]);
		
		        if (!IsINT(atom) && !IsFLOAT(atom)) {
		        	throw IncorrectArgumentTypeException(CloneNode(results[i]), name);
		        }
		
		    	if (i == 0) {
		        	if (IsINT(atom)) {
		        		ans = stoi(atom->value);
		    		} else if (IsFLOAT(atom)) {
		            ans = stod(atom->value);
		            hasFloat = true;
		        	}
		        	continue;
		        }
		
		    	if (IsINT(atom)) {
		        	int value = stoi(atom->value);
		        	if (name == "+") ans += value;
		        	else if (name == "-") ans -= value;
		        	else if (name == "*") ans *= value;
		        	else if (name == "/") {
		        	if (value == 0) throw DivisionByZeroException();
		            ans /= value;
		        	}
		    	} else if (IsFLOAT(atom)) {
		    	double value = stod(atom->value);
		    	hasFloat = true;
		        if (name == "+") ans += value;
		        else if (name == "-") ans -= value;
		        else if (name == "*") ans *= value;
		        else if (name == "/") {
		        if (value == 0.0f) throw DivisionByZeroException();
		        	ans /= value;
		        }
		        }
	      	}
	
	    	if (hasFloat) {
	    		ostringstream oss;
	    		oss << fixed << setprecision(3) << ans;
	        	return make_shared<ATOMNode>(oss.str(), FLOAT);
	      	} else {
	    	return make_shared<ATOMNode>(to_string(static_cast<int>(ans)), INT);
	    	}
		} // Calculator
		
	    shared_ptr<ASTNode> EvalComparison(shared_ptr<ASTNode> node, string name, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable) {
			vector<shared_ptr<ASTNode>> parameters = GetRawParameters(node, name, funcNode);
	      
	    	if (parameters.size() < 2) {
	    		throw WrongNumberOfArgumentException(name);
	    	}
	
			vector<shared_ptr<ASTNode>> results = EvalAllParameters(parameters, funcNode, localSymbolTable);
			
			for ( int i = 0 ; i < results.size() ; i = i + 1 ) {
		        if (results[i]->type != ATOM ) {
		          throw IncorrectArgumentTypeException(CloneNode(results[i]), name);
		        }
		        shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(results[i]);
		
		        if ( ( !IsINT(atom) && !IsFLOAT(atom) ) ) {
		          throw IncorrectArgumentTypeException(CloneNode(results[i]), name);
		        }
			} // for
			
			for (int i = 0; i < results.size() - 1; ++i) {
		        shared_ptr<ATOMNode> atom1 = static_pointer_cast<ATOMNode>(results[i]);
		        shared_ptr<ATOMNode> atom2 = static_pointer_cast<ATOMNode>(results[i + 1]);
		        float val1 = stof(atom1->value);
		        float val2 = stof(atom2->value);
		
		        if (name == ">" && !(val1 > val2)) return make_shared<ATOMNode>("nil", NIL);
		        else if (name == ">=" && !(val1 >= val2)) return make_shared<ATOMNode>("nil", NIL);
		        else if (name == "<" && !(val1 < val2)) return make_shared<ATOMNode>("nil", NIL);
		        else if (name == "<=" && !(val1 <= val2)) return make_shared<ATOMNode>("nil", NIL);
		        else if (name == "=" && !(val1 == val2)) return make_shared<ATOMNode>("nil", NIL);
	      	}
	
	      return make_shared<ATOMNode>("#t", T);
	    } // EvalComparison
			
		shared_ptr<ASTNode> EvalNot(shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable) {
		    vector<shared_ptr<ASTNode>> parameters = GetRawParameters(node, "not", funcNode);
		
		    if (parameters.size() != 1) {
		        throw WrongNumberOfArgumentException("not");
		    }
		
		    vector<shared_ptr<ASTNode>> results = EvalAllParameters(parameters, funcNode, localSymbolTable);
		    bool isFalse = false;
		
		    if (results[0]->type == ATOM) {
		        shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(results[0]);
		        if (IsNIL(atom)) {
		            isFalse = true;
		        }
		    }
		
		    if (isFalse) {
		        return make_shared<ATOMNode>("#t", T);
		    } else {
		        return make_shared<ATOMNode>("nil", NIL);
		    }
		} // EvalNot 
		
		shared_ptr<ASTNode> EvalAnd(shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable) {
		    vector<shared_ptr<ASTNode>> parameters = GetRawParameters(node, "and", funcNode);
		
		    if (parameters.size() < 2) {
		        throw WrongNumberOfArgumentException("and");
		    }
		
		    shared_ptr<ASTNode> lastResult = nullptr;
		
		    for (const auto& param : parameters) {
		    	
		        shared_ptr<ASTNode> result = nullptr ;
		        
				try {
		        	result = Eval(param, dontCareBool, ignore, false, localSymbolTable);
				} catch ( NoReturnValueException& e ){
					if ( !e.GetClassified())
						throw UnboundConditionException(e.GetNode()) ;
					else 
						throw ;
				} // catch
		        
				if (result->type == ATOM) {
		            shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(result);
		            if (IsNIL(atom)) {
		                return make_shared<ATOMNode>("nil", NIL);
		            }
		        }
		
		        lastResult = result;
		    }
		
		    return CloneNode(lastResult);
		} // EvalAnd()

		shared_ptr<ASTNode> EvalOR(shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable) {
		    vector<shared_ptr<ASTNode>> parameters = GetRawParameters(node, "or", funcNode);
		
		    if (parameters.size() < 2) {
		        throw WrongNumberOfArgumentException("or");
		    }
		
		    for (const auto& param : parameters) {
		    	
		        shared_ptr<ASTNode> result = nullptr ;
		        
				try {
		        	result = Eval(param, dontCareBool, ignore, false, localSymbolTable);
				} catch ( NoReturnValueException& e ){
					if ( !e.GetClassified() )
						throw UnboundConditionException(e.GetNode()) ;
					else 
						throw ;
				} // catch
		
		        if (result->type != ATOM) {
		            // CONS or other 非 nil or lambda → true
		            return CloneNode(result);
		        } else {
		            shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(result);
		            if (!IsNIL(atom)) {
		                return CloneNode(result);
		            }
		        }
		    }
		
		    // 全部都是 nil
		    return make_shared<ATOMNode>("nil", NIL);
		} // EvalOR()
		
		shared_ptr<ASTNode> EvalStringAppend(shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable) {
		    vector<shared_ptr<ASTNode>> parameters = GetRawParameters(node, "string-append", funcNode);
		
		    if (parameters.size() < 2) {
		        throw WrongNumberOfArgumentException("string-append");
		    }
		
		    vector<shared_ptr<ASTNode>> results = EvalAllParameters(parameters, funcNode, localSymbolTable);
		    string ans = "";
		
		    for (const auto& res : results) {
		        if (res->type != ATOM) {
		            throw IncorrectArgumentTypeException(CloneNode(res), "string-append");
		        }
		
		        shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(res);
		        if (!IsString(atom)) {
		            throw IncorrectArgumentTypeException(CloneNode(res), "string-append");
		        }
		
		        ans += atom->value;
		    }
		
		    return make_shared<ATOMNode>(ans, STRING);
		} // EvalStringAppend()

		shared_ptr<ASTNode> EvalStringCompare(shared_ptr<ASTNode> node, const string& name, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable) {
		    vector<shared_ptr<ASTNode>> parameters = GetRawParameters(node, name, funcNode);
		
		    if (parameters.size() < 2) {
		        throw WrongNumberOfArgumentException(name);
		    }
		
		    vector<shared_ptr<ASTNode>> results = EvalAllParameters(parameters, funcNode, localSymbolTable);
		
		    for (const auto& res : results) {
		        if (res->type == ATOM) {
		            shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(res);
		            if (!IsString(atom)) {
		                throw IncorrectArgumentTypeException(CloneNode(res), name);
		            }
		        }
		    }
		
		    for (int i = 0; i < results.size() - 1; ++i) {
		        if (results[i]->type != ATOM || results[i + 1]->type != ATOM) {
		            throw IncorrectArgumentTypeException(CloneNode(results[i]->type != ATOM ? results[i] : results[i + 1]), name);
		        }
		
		        shared_ptr<ATOMNode> atom1 = static_pointer_cast<ATOMNode>(results[i]);
		        shared_ptr<ATOMNode> atom2 = static_pointer_cast<ATOMNode>(results[i + 1]);
		
		        string val1 = atom1->value;
		        string val2 = atom2->value;
		
		        if (name == "string>?") {
		            if (!(val1 > val2)) {
		                return make_shared<ATOMNode>("nil", NIL);
		            } // if
		        }// if
				else if (name == "string<?") {
		            if (!(val1 < val2)) {
		                return make_shared<ATOMNode>("nil", NIL);
		            }// if
		        } // else if
				else if (name == "string=?") {
		            if (!(val1 == val2)) {
		                return make_shared<ATOMNode>("nil", NIL);
		            }// if
		        } // else if
		    } // for i
		
		    return make_shared<ATOMNode>("#t", T);
		} // EvalStringCompare()

		shared_ptr<ASTNode> EvalEqv2(shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable ) {
		    vector<shared_ptr<ASTNode>> results = GetRawParameters(node, "eqv?", funcNode);
		
		    if (results.size() != 2) {
		        throw WrongNumberOfArgumentException("eqv?");
		    }
		
		    shared_ptr<ASTNode> first = results[0];
		    shared_ptr<ASTNode> second = results[1];
		    shared_ptr<ASTNode> Cvalue1 = first;
		    shared_ptr<ASTNode> Cvalue2 = second;
		    bool isEqual = false;
		
		    if (first->type == ATOM) {
		        shared_ptr<ATOMNode> atom1 = static_pointer_cast<ATOMNode>(first);
		        if (IsBoundSymbol(atom1, localSymbolTable)) {
		            Cvalue1 = GetRawVariable(atom1->value, localSymbolTable);
		        } // if
		    } else {
		    	try {
		    		Cvalue1 = Eval( first, dontCareBool, ignore, false, localSymbolTable ) ;
				} catch ( NoReturnValueException& e ) {
					if ( !e.GetClassified() )
						throw UnboundParameterException(e.GetNode()) ;
					else
						throw ;
				} // catch
			}
		
		    if (second->type == ATOM) {
		        shared_ptr<ATOMNode> atom2 = static_pointer_cast<ATOMNode>(second);
		        if (IsBoundSymbol(atom2, localSymbolTable)) {
		            Cvalue2 = GetRawVariable(atom2->value, localSymbolTable);
		        } // if
		    } else {
		    	try {
		    		Cvalue2 = Eval( second, dontCareBool, ignore, false, localSymbolTable ) ;
				} catch ( NoReturnValueException& e ) {
					if ( !e.GetClassified() )
						throw UnboundParameterException(e.GetNode()) ;
					else
						throw ;
				} // catch
			}
		
		    if (Cvalue1->type == ATOM && Cvalue2->type == ATOM) {
		        shared_ptr<ATOMNode> atom1 = static_pointer_cast<ATOMNode>(Cvalue1);
		        shared_ptr<ATOMNode> atom2 = static_pointer_cast<ATOMNode>(Cvalue2);
		
		        if (IsString(atom1) && IsString(atom2)) {
		            isEqual = (Cvalue1 == Cvalue2);  // 指向同一個字串節點才為 true
		        } // if
				else if (atom1->value == atom2->value) {
		            isEqual = true;
		        } // else if
		    } // if 
			else {
		        if (Cvalue1 == Cvalue2)
		            isEqual = true;
		    } // else
			
		    if (isEqual)
		        return make_shared<ATOMNode>("#t", T);
		    else
		        return make_shared<ATOMNode>("nil", NIL);
		} // EvalEqv2()

		// local variable (已完成, no Eval)
		bool IsNodeEqual(shared_ptr<ASTNode> node1, shared_ptr<ASTNode> node2) {
		    if (!node1 || !node2) return false;
		
		    if (node1->type == ATOM && node2->type == ATOM) {
		        shared_ptr<ATOMNode> atom1 = static_pointer_cast<ATOMNode>(node1);
		        shared_ptr<ATOMNode> atom2 = static_pointer_cast<ATOMNode>(node2);
		        return atom1->value == atom2->value;
		    }
		    else if (node1->type == CONS && node2->type == CONS) {
		        shared_ptr<ConsNode> cons1 = static_pointer_cast<ConsNode>(node1);
		        shared_ptr<ConsNode> cons2= static_pointer_cast<ConsNode>(node2);
		        return IsNodeEqual(cons1->left, cons2->left) && IsNodeEqual(cons1->right, cons2->right);
		    }
		    else if (node1->type == LAMBDA && node2->type == LAMBDA) {
		    	 // 即使兩個 lambda 結構相同，也視為不同，因此return false 
		    	return false ;
//		        shared_ptr<ConsNode> lambda1 = static_pointer_cast<ConsNode>(node1);
//		        shared_ptr<ConsNode> lambda2= static_pointer_cast<ConsNode>(node2);
//		        return IsNodeEqual(lambda1->variables, lambda2->variables) && IsNodeEqual(lambda1->functions, lambda2->functions);
		    }
		    return false;
		} // IsNodeEqual()

		shared_ptr<ASTNode> EvalEqual(shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable) {
		    vector<shared_ptr<ASTNode>> parameters = GetRawParameters(node, "equal?", funcNode);
		
		    if (parameters.size() != 2) {
		        throw WrongNumberOfArgumentException("equal?");
		    }
		
		    vector<shared_ptr<ASTNode>> results = EvalAllParameters(parameters, funcNode, localSymbolTable);
		    shared_ptr<ASTNode> first = results[0];
		    shared_ptr<ASTNode> second = results[1];
		
		    bool isEqual = IsNodeEqual(first, second);
		
		    return make_shared<ATOMNode>(isEqual ? "#t" : "nil", isEqual ? T : NIL);
		} // EvalEqual

		shared_ptr<ASTNode> EvalLet(shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable ) {
			//LetFormatException
			vector<shared_ptr<ASTNode>> results = GetRawParameters(node, "let", funcNode);
			
		    if (results.size() < 2) {
		        throw LetFormatException(funcNode);
		    }
			
			// 處理 variable 以及 value 的值( 以AST tree的方式存在 variableHead, valueHead 裡面 ) 
			vector<shared_ptr<ASTNode>> Variables ;
			vector<shared_ptr<ASTNode>> Values ;
			vector<shared_ptr<ASTNode>> Functions ;
			GetVariableAndValue( results[0], funcNode, Variables, Values, localSymbolTable ) ;
			//處理lambda中的functions 

			for ( int i = 1 ; i < results.size() ; i++ ) {

				Functions.push_back( results[i] ) ;
			} // for
			
			return ProceedLetFunction( Variables, Values, Functions, funcNode, localSymbolTable ) ;	
			 
		} // EvalLet
		
		// no return error done! (next)
		shared_ptr<ASTNode> ProceedLetFunction( vector<shared_ptr<ASTNode>> Variables, vector<shared_ptr<ASTNode>> Values, vector<shared_ptr<ASTNode>> Functions, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> >  localSymbolTable ){
			//	let 會建立 全新的區域變數表（symbol table）
			//	每個 let 都會複製一份 symbol table，然後加上自己的變數與對應值（像是 x, y, a）。
			//	每層 let 不會改動外層變數，也不會影響外層。
			
			// 建立專屬於let的localSymbolTable( 由於此 ProceedLetFunction() 是傳值( call by value ) 故不影響其他 localSymbolTable
			// Variables.size() == Value.size()
			for( int i = 0 ; i < Variables.size() ; i++ ) {
				// Variables 全都是ATOM 在 GetVariableAndValue() 檢查過 
				shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(Variables[i]) ;
				localSymbolTable[ atom->value ] = Values[i] ; 
			} // for 
			
			shared_ptr<ASTNode> Ans = nullptr ;
			
			for( int i = 0 ; i < Functions.size() ; i++ ) {
				try {
					Ans = Eval( Functions[i], dontCareBool, ignore, false, localSymbolTable ) ; 
				} catch ( NoReturnValueException& e ) {
					if ( i != Functions.size() - 1 ) {
							continue ;
					} // if
					else {
						if ( !e.GetClassified() ) { 
							throw NoReturnValueException(e.GetNode());
						} 
						else
							throw ;
					} // else 
				} // catch
				
			} // for
			
			return Ans ;
		} // let


// lambda 算是一個function 若有 no return value 的情況就是他要輸出 Error massage 
// EvalCons 的 funcNode 讀取到 lambda node 要計算 lambda的值( 使用者應給參數值 )
		shared_ptr<ASTNode> EvalLambdaCall( shared_ptr<ASTNode> value, shared_ptr<LambdaNode> Lnode, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable ) {
			
			map< string, shared_ptr<ASTNode> > newTable ; // 新的區域變數 
			
//         先找到有幾個變數才能定出	results.size() 的條件	
		    vector<shared_ptr<ASTNode>> Variables = GetRawParameters(Lnode->variables, "lambda procedure", funcNode); // lambda的variables全都是atom 否則在這之前會throw error ( EvalLambdaFunc ) 
			vector<shared_ptr<ASTNode>> Functions = GetRawParameters(Lnode->functions, "lambda procedure", funcNode); // 在此前並沒有Eval過，故Function內部即使是錯的，目前還不會丟Error 
			vector<shared_ptr<ASTNode>> Value = GetRawParameters(value, "lambda precedure", funcNode); // Raw Value
		    
			if ( Value.size() != Variables.size() ) {
		        throw WrongNumberOfArgumentException(Lnode->value);
		    } // if 
		    
		    // Evaluate Value
		    for ( int i = 0 ; i < Value.size() ; i++ ) {
		    	// 跟lambda屬於同一層級，應用上一層的local variables 
		    	try {
		    		Value[i] = Eval(Value[i], dontCareBool, ignore, false, localSymbolTable) ; // Value Evaluated ！ 
				} catch ( NoReturnValueException& e ) {
					if ( !e.GetClassified() )
						throw UnboundParameterException(e.GetNode());
					else 
						throw ;
				} // catch
		    	
			} // for
		    
		    // 建立新的區域變數 
		    for ( int i = 0 ; i < Variables.size() ; i++ ) {
		    	shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>( Variables[i] ) ;
		    	newTable[atom->value] = Value[i] ;
			} // for
		    
		    // 使用newTable call Eval functions
			shared_ptr<ASTNode> Ans = nullptr ;
			for ( int i = 0 ; i < Functions.size() ; i++ ) {
		    	try {
		    		Ans = Eval(Functions[i], dontCareBool, ignore, false, newTable ) ;
				} catch ( NoReturnValueException& e ) {
					if ( i != Functions.size() - 1 )
						continue ;
					else {
						if ( !e.GetClassified() ){
							throw NoReturnValueException(funcNode);
//							if ( Lnode->value == "lambda" )
//								throw NoReturnValueException(funcNode);
//							else 
//								throw NoReturnValueException(funcNode,true);
						} // if
						else {
							throw ;
						}
					} // else
				} // catch

			} // for
			
			return Ans ;
		} // EvalLambdaCall

// lambda 組成 function <precedure lambda> 
		shared_ptr<ASTNode> EvalLambdaFunc( shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode ) {
		    vector<shared_ptr<ASTNode>> results = GetRawParameters(node, "lambda", funcNode);
			
		    if (results.size() < 2) { 
		        throw LambdaFormatException(funcNode);
		    } // if
		    vector<shared_ptr<ASTNode>> variables = GetLambdaVariables(results[0], funcNode);

		    shared_ptr<ASTNode> variableHead = nullptr ;
		    if ( variables.size() == 0 ) {
		    	variableHead = make_shared<ATOMNode>("nil", NIL) ;
			} // if
			else {
				
			    if ( variables[0]->type != ATOM ) {
			    	throw LambdaFormatException(funcNode);
				} // if
				shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(variables[0]) ;
					if ( IsLiteral(atom) || IsFunctionName(atom->value) ){
						throw LambdaFormatException(funcNode);
					}
						
						
			    variableHead = make_shared<ConsNode>(variables[0]) ;
			    
			    shared_ptr<ConsNode> currentVariable = static_pointer_cast<ConsNode>(variableHead) ;
				for ( int i = 1 ; i < variables.size() ; i ++ ) {
			    	if ( variables[i]->type != ATOM ) {
			    		throw LambdaFormatException(funcNode);
					} // if
					
					shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(variables[i]) ;
					
					if ( IsLiteral(atom) || IsFunctionName(atom->value) ) {
						throw LambdaFormatException(funcNode);
					}
						
						
					currentVariable->right = make_shared<ConsNode>( variables[i] ) ;
					currentVariable = static_pointer_cast<ConsNode>(currentVariable->right) ;
				} // for
				currentVariable->right = make_shared<ATOMNode>( "nil", NIL ) ;
			}

			
			
			shared_ptr<ASTNode> functionHead = make_shared<ConsNode>(results[1]) ;
			shared_ptr<ConsNode> currentFunction = static_pointer_cast<ConsNode>(functionHead) ;
			
			for( int i = 2 ; i < results.size() ; i ++ ) {
				currentFunction->right = make_shared<ConsNode>( results[i] ) ;
				currentFunction = static_pointer_cast<ConsNode>(currentFunction->right) ;
			} // for
			currentFunction->right = make_shared<ATOMNode>( "nil", NIL ) ;
			
			// shared_ptr<LambdaNode> lambda = make_shared<LambdaNode>(variableHead, functionHead) ;
			return make_shared<LambdaNode>(variableHead, functionHead) ;
			
		} // EvalLambdaFunc
		
		shared_ptr<ASTNode> EvalVerbose( shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable ) {
			vector<shared_ptr<ASTNode>> parameters = GetRawParameters(node, "verbose", funcNode);
	
	    	if (parameters.size() != 1) {
	    		throw WrongNumberOfArgumentException("verbose");
	    	}
			vector<shared_ptr<ASTNode>> results = EvalAllParameters(parameters, funcNode, localSymbolTable);
			
			if ( results[0]->type == ATOM ) {
				shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(results[0]) ;
				if ( IsNIL(atom) ) {
					Verbose = false ;
					return make_shared<ATOMNode>("nil", NIL);
				} // if 
			} // if

			Verbose = true ;
			return make_shared<ATOMNode>("#t", T);

		} // EvalVerbose()
		
		shared_ptr<ASTNode> EvalIsVerbose( shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable ) {
			vector<shared_ptr<ASTNode>> parameters = GetRawParameters(node, "verbose?", funcNode);
	
	    	if (parameters.size() != 0) {
	    		throw WrongNumberOfArgumentException("verbose?");
	    	}
			
			if ( Verbose )
				return make_shared<ATOMNode>("#t", T);
			else
				return make_shared<ATOMNode>("nil", NIL);

		} // EvalIsVerbose()
		
		shared_ptr<ASTNode> EvalCreateErrorObj( shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable ) {
			
			vector<shared_ptr<ASTNode>> parameters = GetRawParameters( node, "create-error-object", funcNode ) ;
			
			if ( parameters.size() != 1 ) {
				throw WrongNumberOfArgumentException( "create-error-object" ) ;
			}
			
			vector< shared_ptr<ASTNode> > results = EvalAllParameters(parameters, funcNode, localSymbolTable);
			
			if ( results[0]->type != ATOM ) {
				throw IncorrectArgumentTypeException(CloneNode(results[0]), "create-error-object");
			}
			else {
				shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(results[0]) ;
				if ( atom->tokentype != STRING )
					throw IncorrectArgumentTypeException(CloneNode(results[0]), "create-error-object");
				else
					return make_shared<ATOMNode>(atom->value, ERROR);
			}
			
			throw runtime_error("ERROR in EvalCreateErrorObj!") ;
			
			
		} // EvalCreateErrorObj()
		
		shared_ptr<ASTNode> EvalIsErrorObj( shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable ) {
			
			vector<shared_ptr<ASTNode>> parameters = GetRawParameters( node, "error-object?", funcNode ) ;
			
			if ( parameters.size() != 1 ) {
				throw WrongNumberOfArgumentException( "error-object?" ) ;
			}
			
			vector< shared_ptr<ASTNode> > results = EvalAllParameters(parameters, funcNode, localSymbolTable);
			
			if ( results[0]->type == ATOM ) {
				shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(results[0]) ;
				if ( atom->tokentype == ERROR ) {
					return make_shared<ATOMNode>("#t", T);
				} // if 
			} // if
			
			return make_shared<ATOMNode>("nil", NIL);
			
			
		} // EvalIsErrorObj()
		
	    shared_ptr<ASTNode> EvalRead( shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable ) {
	    	
	    	vector<shared_ptr<ASTNode>> parameters = GetRawParameters(node, "read", funcNode);
	
	    	if (!parameters.empty()) {
	    		throw WrongNumberOfArgumentException("read");
	    	}
			
			
			shared_ptr<ASTNode> result ;
			try {
				result = g_parser.parse() ;
			} catch ( const NoMoreInputException& e ) {
				return make_shared<ATOMNode>("ERROR : END-OF-FILE encountered when there should be more input", ERROR) ; 
			} catch ( const UnExpectedTokenException& e ) {
				string str = "ERROR (unexpected token) : atom or '(' expected when token at Line " + to_string(e.getLine()) +" Column "+ to_string(e.getColumn()) + " is >>"+ e.getStr() + "<<" ;
				return make_shared<ATOMNode>(str, ERROR) ; 
			} catch ( const NoClosingQuoteException& e ){
	          	return make_shared<ATOMNode>("ERROR (no closing quote) : END-OF-LINE encountered at Line " + to_string(e.getLine()) + " Column " + to_string(e.getColumn()), ERROR) ; 
			} catch ( const MissingRightParenException& e ) {
	          	return make_shared<ATOMNode>("ERROR (unexpected token) : ')' expected when token at Line " + to_string(e.getLine()) + " Column " + to_string(e.getColumn()) + " is >>" + e.getStr() + "<<", ERROR) ;
			}
			
			return result ;
			
	    } // EvalRead()

	    shared_ptr<ASTNode> EvalWrite( shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable ) {
			
			vector<shared_ptr<ASTNode>> parameters = GetRawParameters( node, "write", funcNode ) ;
			
			if ( parameters.size() != 1 ) {
				throw WrongNumberOfArgumentException( "write" ) ;
			}
			
			vector< shared_ptr<ASTNode> > results = EvalAllParameters(parameters, funcNode, localSymbolTable);
			
			printAST(results[0],false,true,0,true,true) ;
			return CloneNode( results[0] );
	    } // EvalWrite()
	    
	    shared_ptr<ASTNode> EvalDisplayString( shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable ) {
			
			vector<shared_ptr<ASTNode>> parameters = GetRawParameters( node, "display-string", funcNode ) ;
			
			if ( parameters.size() != 1 ) {
				throw WrongNumberOfArgumentException( "display-string" ) ;
			}
			
			vector< shared_ptr<ASTNode> > results = EvalAllParameters(parameters, funcNode, localSymbolTable);
			
			if ( results[0]->type != ATOM ) {
				throw IncorrectArgumentTypeException(CloneNode(results[0]), "display-string");
			}
			else {
				shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(results[0]) ;
				if (atom->tokentype != STRING && atom->tokentype != ERROR)
					throw IncorrectArgumentTypeException(CloneNode(results[0]), "display-string");
				else {
					printAST(results[0],true,false,0,true,true) ;
					return CloneNode( results[0] );
				}
			}
			
			throw runtime_error("ERROR in EvalCreateErrorObj!") ;
	    } // EvalDisplayString()
	    
	    shared_ptr<ASTNode> EvalNewLine( shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable ) {
	    	vector<shared_ptr<ASTNode>> parameters = GetRawParameters(node, "newline", funcNode);
	
	    	if (!parameters.empty()) {
	    		throw WrongNumberOfArgumentException("newline");
	    	}
			
			cout << "\n" ;
			return make_shared<ATOMNode>("nil", NIL);
		} // EvalNewLine
		
	    // EvalSymbolToString symbol->string number->string
	    shared_ptr<ASTNode> EvalSymbolToString( shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable ) {
			
			vector<shared_ptr<ASTNode>> parameters = GetRawParameters( node, "symbol->string", funcNode ) ;
			
			if ( parameters.size() != 1 ) {
				throw WrongNumberOfArgumentException( "symbol->string" ) ;
			}
			
			vector< shared_ptr<ASTNode> > results = EvalAllParameters(parameters, funcNode, localSymbolTable);
			
			if ( results[0]->type != ATOM ) {
				throw IncorrectArgumentTypeException(CloneNode(results[0]), "symbol->string");
			}
			else {
				shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(results[0]) ;
				if ( atom->tokentype != SYMBOL )
					throw IncorrectArgumentTypeException(CloneNode(results[0]), "symbol->string");
				else {
					return make_shared<ATOMNode>(atom->value, STRING) ;
				}
			}
			
			throw runtime_error("ERROR in EvalSymbolToString!") ;
	    } // EvalSymbolToString()  
	    	    
	    shared_ptr<ASTNode> EvalNumToString( shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable ) {
			
			vector<shared_ptr<ASTNode>> parameters = GetRawParameters( node, "number->string", funcNode ) ;
			
			if ( parameters.size() != 1 ) {
				throw WrongNumberOfArgumentException( "number->string" ) ;
			}
			
			vector< shared_ptr<ASTNode> > results = EvalAllParameters(parameters, funcNode, localSymbolTable);
			
			if ( results[0]->type != ATOM ) {
				throw IncorrectArgumentTypeException(CloneNode(results[0]), "number->string");
			}
			else {
				shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(results[0]) ;
				if ( atom->tokentype != INT && atom->tokentype != FLOAT )
					throw IncorrectArgumentTypeException(CloneNode(results[0]), "number->string");
				else {
					return make_shared<ATOMNode>(atom->value, STRING) ;
				}
			}
			
			throw runtime_error("ERROR in EvalSymbolToString!") ;
	    } // EvalNumToString()
		
	    shared_ptr<ASTNode> Evaluate( shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable ) {
			
			vector<shared_ptr<ASTNode>> parameters = GetRawParameters( node, "eval", funcNode ) ;
			
			if ( parameters.size() != 1 ) {
				throw WrongNumberOfArgumentException( "eval" ) ;
			}
			
			vector< shared_ptr<ASTNode> > results = EvalAllParameters(parameters, funcNode, localSymbolTable);
			
			return Eval(results[0], dontCareBool, ignore, true, localSymbolTable ) ;
			
	    } // Evaluate()
	    
	    shared_ptr<ASTNode> EvalSet( shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable ) { // ( 寫到這 ) ( 取上一版define ) 
	    
		  vector<shared_ptr<ASTNode>> results = GetRawParameters( node, "set!", funcNode );
	
	      if (results.size() != 2)
	        throw SetFormatException();
	
	      shared_ptr<ASTNode> variable = results[0];
	      shared_ptr<ASTNode> value = nullptr;
	
	      if (variable->type != ATOM)
	        throw SetFormatException();
	
	      shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(variable);
	      string varName = atom->value;
	
	      if (IsLiteral(atom) || IsFunctionName(varName))
	        throw SetFormatException();
	
	      // handle (define b a) → b 與 a 指向同一份物件
	      shared_ptr<ATOMNode> tempAtom = nullptr;
	      if (results[1]->type == ATOM)
	        tempAtom = static_pointer_cast<ATOMNode>( results[1] );
	
	      if (tempAtom && IsBoundSymbol(tempAtom, localSymbolTable)) {
	        value = GetRawVariable(tempAtom->value, localSymbolTable);  // no clone
	      } else {
	        value = Eval( results[1], dontCareBool, ignore, false, localSymbolTable );
//	        symbolTable[varName] = CloneNode(value);  // clone for safety
	      }
	
		  if (localSymbolTable.find(varName) != localSymbolTable.end()) {
			  localSymbolTable[varName] = CloneNode(value);
		  } else {
			  symbolTable[varName] = CloneNode(value);
		  }
	
	      return CloneNode(value);
		} // EvalSet
		
		// local variable (已完成, no Eval)
		void EvalExit(shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode) {
		    vector<shared_ptr<ASTNode>> parameters = GetRawParameters(node, "exit", funcNode);
		
		    if (!parameters.empty()) {
		        throw WrongNumberOfArgumentException("exit");
		    }
		
		    throw ExitException();
		}
		
	    shared_ptr<ASTNode> EvalBuiltIn(string name, shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable) {
	      if (name == "#<procedure cons>") return EvalCreateCons(node, funcNode, localSymbolTable);
	      else if (name == "#<procedure list>") return EvalList(node, funcNode, localSymbolTable);
	      else if (name == "#<procedure car>") return EvalCarCdr(node, "car", funcNode, localSymbolTable);
	      else if (name == "#<procedure cdr>") return EvalCarCdr(node, "cdr", funcNode, localSymbolTable);
	      else if (name == "#<procedure atom?>") return EvalIsAtom(node, funcNode, localSymbolTable);
	      else if (name == "#<procedure pair?>") return EvalIsPair(node, funcNode, localSymbolTable);
	      else if (name == "#<procedure list?>") return EvalIsList(node, funcNode, localSymbolTable);
	      else if (name == "#<procedure null?>") return EvalIsNull(node, funcNode, localSymbolTable);
	      else if (name == "#<procedure integer?>") return EvalIsInteger(node, funcNode, localSymbolTable);
	      else if (name == "#<procedure real?>") return EvalIsRealAndNumber(node, "real?", funcNode, localSymbolTable);
	      else if (name == "#<procedure number?>") return EvalIsRealAndNumber(node, "number?", funcNode, localSymbolTable);
	      else if (name == "#<procedure string?>") return EvalIsString(node, funcNode, localSymbolTable);
	      else if (name == "#<procedure boolean?>") return EvalIsBoolean(node, funcNode, localSymbolTable);
	      else if (name == "#<procedure symbol?>") return EvalIsSymbol(node, funcNode, localSymbolTable);
	      else if (name == "#<procedure +>") return Calculator(node, "+", funcNode, localSymbolTable);
	      else if (name == "#<procedure ->") return Calculator(node, "-", funcNode, localSymbolTable);
	      else if (name == "#<procedure *>") return Calculator(node, "*", funcNode, localSymbolTable);
	      else if (name == "#<procedure />") return Calculator(node, "/", funcNode, localSymbolTable);
	      else if (name == "#<procedure not>") return EvalNot(node, funcNode, localSymbolTable);
	      else if (name == "#<procedure and>") return EvalAnd(node, funcNode, localSymbolTable);
	      else if (name == "#<procedure or>") return EvalOR(node, funcNode, localSymbolTable);
	      else if (name == "#<procedure >>") return EvalComparison(node, ">", funcNode, localSymbolTable);
	      else if (name == "#<procedure >=>") return EvalComparison(node, ">=", funcNode, localSymbolTable);
	      else if (name == "#<procedure <>") return EvalComparison(node, "<", funcNode, localSymbolTable);
	      else if (name == "#<procedure <=>") return EvalComparison(node, "<=", funcNode, localSymbolTable);
	      else if (name == "#<procedure =>") return EvalComparison(node, "=", funcNode, localSymbolTable);
	      else if (name == "#<procedure string-append>") return EvalStringAppend(node, funcNode, localSymbolTable);
	      else if (name == "#<procedure string>?>") return EvalStringCompare(node, "string>?", funcNode, localSymbolTable);
	      else if (name == "#<procedure string<?>") return EvalStringCompare(node, "string<?", funcNode, localSymbolTable);
	      else if (name == "#<procedure string=?>") return EvalStringCompare(node, "string=?", funcNode, localSymbolTable);
	      else if (name == "#<procedure eqv?>") return EvalEqv2(node, funcNode, localSymbolTable);
	      else if (name == "#<procedure equal?>") return EvalEqual(node, funcNode, localSymbolTable);
	      else
	      	throw runtime_error("ERROR in EvalBuiltIn");
	    } // EvalBuiltIn
		 
		void GetVariableAndValue(shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode, vector<shared_ptr<ASTNode>> & Variables, vector<shared_ptr<ASTNode>> & Values, map< string, shared_ptr<ASTNode> > & localSymbolTable ) {
			vector<shared_ptr<ASTNode>> results = GetRawParameters(node, "let", funcNode);
			
			
			// 沒有變數的情況 Variables.size() == 0 and Values.size() == 0
			if (results.size() == 0) return;
	        	
	        for( int i = 0 ; i < results.size() ; i ++ ) {
	        	vector<shared_ptr<ASTNode>> variableAndvalue = GetTwoElementsList( results[i], funcNode ) ;
	        	
				if ( variableAndvalue.size() != 2 ) {
					throw LetFormatException(funcNode);
				} // if
				if ( variableAndvalue[0]->type != ATOM ) {
					throw LetFormatException(funcNode);
				}
				shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(variableAndvalue[0]);
				if ( IsLiteral(atom) || IsFunctionName(atom->value) )
		        	throw LetFormatException(funcNode);
		        	
		        Variables.push_back( variableAndvalue[0] ) ;
		        Values.push_back( variableAndvalue[1] ) ;
		       
			} // for
			
			for( int i = 0 ; i < Values.size() ; i ++ ) {
				try {
					Values[i] = Eval( Values[i], dontCareBool, ignore, false, localSymbolTable ) ;
				} catch ( NoReturnValueException& e) {
					if ( !e.GetClassified() )
						throw NoReturnValueException( e.GetNode(), true );
					else 
						throw ;
				} // catch		
			} // for
			
		} // GetVariableAndValue
		
		vector<shared_ptr<ASTNode>> GetLambdaVariables(shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode) {
			vector<shared_ptr<ASTNode>> results ;
			
			if (node->type == ATOM) {
		        shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(node);
		        if ( IsNIL ( atom ) ) 
		            return results;
		        else {
		        	throw LambdaFormatException(funcNode);
				} // else
		    } // if
		    else if ( node->type == LAMBDA ) {
				throw LambdaFormatException(funcNode);
			} // else if
			
		    while ( node->type == CONS ) {
		        shared_ptr<ConsNode> cons = static_pointer_cast<ConsNode>(node);
		        results.push_back( CloneNode(cons->left) );  // 不做 Eval！
		        node = cons->right;
		    } // while
		    
			if (node->type == ATOM) {
		        shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(node);
		        if ( IsNIL ( atom ) ) 
		            return results;
		        else {
		        	throw LambdaFormatException(funcNode);
				} // else
		    } // if
			else if ( node->type == LAMBDA ) {
				throw LambdaFormatException(funcNode);
			} // else if
			
			return results;
		    
		} // GetLambdaVariable

		vector<shared_ptr<ASTNode>> GetTwoElementsList(shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode) {
			
			vector<shared_ptr<ASTNode>> results ;
			
			if (node->type == ATOM) {
		        shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(node);
		        if ( IsNIL ( atom ) ) 
		            return results;
		        else {
		        	throw LetFormatException(funcNode);
				} // else
		    } // if
			else if ( node->type == LAMBDA ) {
				throw LetFormatException(funcNode);
			} // else if
		    
		    while ( node->type == CONS ) {
		        shared_ptr<ConsNode> cons = static_pointer_cast<ConsNode>(node);
		        results.push_back( CloneNode(cons->left) );  // 不做 Eval！
		        node = cons->right;
		    } // while
		    
			if (node->type == ATOM) {
		        shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(node);
		        if ( IsNIL ( atom ) ) 
		            return results;
		        else {
		        	throw LetFormatException(funcNode);
				} // else
		    } // if
			else if ( node->type == LAMBDA ) {
				throw LetFormatException(funcNode);
			} // else if
			

			
			if ( results.size() != 2 )
				throw LetFormatException(funcNode);
				
			return results;
		    
		} // GetTwoElementList

		vector<shared_ptr<ASTNode>> GetRawParameters(shared_ptr<ASTNode> node, string funcName, shared_ptr<ASTNode> funcNode) {
		    
			vector<shared_ptr<ASTNode>> results ;
		
		    if (node->type == ATOM) {
		        shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(node);
		        if ( IsNIL ( atom ) ) 
		            return results;
		        else {
		        	if (funcName == "define2")
		        		throw DefineFormatException();
		        	else
		        		throw NonListException(funcNode);
				} // else
		    } // if
			else if ( node->type == LAMBDA ) {
				throw NonListException(funcNode) ;
			} // else if
		
		    while ( node->type == CONS ) {
		        shared_ptr<ConsNode> cons = static_pointer_cast<ConsNode>(node);
		        results.push_back( CloneNode(cons->left) );  // 不做 Eval！
		        node = cons->right;
		    } // while
		
			if ( node->type == ATOM ) {
				shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(node);
				if ( !IsNIL ( atom ) ) {
					throw NonListException(funcNode) ;
				} //if
			} // if 
			
			// 以防萬一( 在建樹時不會有lambda的出現，故正常情況下lambda不會出現在樹狀結構中 Ex: 不會有cons->right->type == LAMBDA的問題 )
			// ( 而最右邊的節點又會被 DetectRest() 擋下來 ) 
			else if ( node->type == LAMBDA ) {
				throw NonListException(funcNode) ;
			} // else if
		
		    return results;
		} // GetRawParameters
		
		vector<shared_ptr<ASTNode>> EvalAllParameters( vector<shared_ptr<ASTNode>> parameters, shared_ptr<ASTNode> funcNode, map< string, shared_ptr<ASTNode> > & localSymbolTable ) {
			
			vector< shared_ptr<ASTNode> > results ;
			
			for ( int i = 0 ; i < parameters.size() ; i = i + 1 ) {
				try { 
					results.push_back( Eval(parameters[i], dontCareBool, ignore, false, localSymbolTable ) ) ; 
				} catch ( NoReturnValueException& e ) {
					if( !e.GetClassified() )
						throw UnboundParameterException(e.GetNode()) ;
					else {
						throw ;
					}
				} // catch
				
			} // for
			
			return results ;
			
		} // EvalAllParameters()

		vector<shared_ptr<ASTNode>> GetCondAndPos( shared_ptr<ASTNode> node, shared_ptr<ASTNode> funcNode ) {
			
			vector<shared_ptr<ASTNode>> results ;
		
		    if (node->type != CONS) { 
		        throw CondFormatException(funcNode) ;
		    }
		
		    while ( node->type == CONS ) {
		        shared_ptr<ConsNode> cons = static_pointer_cast<ConsNode>(node);
		        results.push_back( CloneNode(cons->left) );  // 不做 Eval！
		        node = cons->right;
		    }
			
			if ( node->type == ATOM ) {
				shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(node);
				if ( !IsNIL ( atom ) ) {
					throw CondFormatException(funcNode) ;
				} // if
			} // if 
			else if ( node->type == LAMBDA ) {
				throw CondFormatException(funcNode) ;
			}
		
		    return results;
				
		} // GetCondAndPos

		shared_ptr<ASTNode> GetVariable( const string& str, map< string, shared_ptr<ASTNode> > & localSymbolTable ) {
			
			auto ans = localSymbolTable.find(str) ;
			if ( ans != localSymbolTable.end() ) {
				return CloneNode( ans->second ) ;
			} // if
			else {
				ans = symbolTable.find(str) ;
				if ( ans != symbolTable.end() ) {
					return CloneNode( ans->second ) ;
				} // if
			} // else 
			
			throw UnboundSymbolException(str) ;

		} // GetVariable

		shared_ptr<ASTNode> GetRawVariable( const string& str, map< string, shared_ptr<ASTNode> > & localSymbolTable ) {
			
			auto ans = localSymbolTable.find(str) ;
			if ( ans != localSymbolTable.end() ) {
				return ans->second ;
			} // if
			else {
				ans = symbolTable.find(str) ;
				if ( ans != symbolTable.end() ) {
					return ans->second ;
				} // if
			} // else 
			
			throw UnboundSymbolException(str) ;

		} // GetRawVariable

		bool IsCorrectList(shared_ptr<ASTNode> node) {
		    while (node && node->type == CONS) {
		        shared_ptr<ConsNode> cons = static_pointer_cast<ConsNode>(node);
		        node = cons->right;
		    }
			
			if ( node && node->type == LAMBDA ) {
				return false ;
			}
		    if (node && node->type == ATOM) {
		        shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(node);
		        return IsNIL(atom);
		    }
		
		    return false;
		} // IsCorrectList

		void DetectRest( shared_ptr<ASTNode> rest, shared_ptr<ASTNode> funcNode ) {
			
			shared_ptr<ASTNode> node = rest ;
			
			if ( node->type == ATOM ) {
				shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(node);
				if ( !IsNIL ( atom ) )
					throw NonListException(funcNode) ;
			} // if 
			else if ( node->type == LAMBDA ){
				throw NonListException(funcNode) ;
			}
			
		    while ( node->type == CONS ) {
		        shared_ptr<ConsNode> cons = static_pointer_cast<ConsNode>(node);
		        node = cons->right;
		    } // while
		
			if ( node->type == ATOM ) {
				shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(node);
				if ( !IsNIL ( atom ) ) {
					throw NonListException(funcNode) ;
				} //if
			} // if 
			else if ( node->type == LAMBDA ){
				throw NonListException(funcNode) ;
			}
		} // DetectRest
		
		bool IsLambdaFunction( string &name, map< string, shared_ptr<ASTNode> > & localSymbolTable ) {
			string key = "" ;
			for ( auto pair : localSymbolTable ) {
				key = pair.first ;
				if ( name == "#<procedure " + key + ">" && pair.second->type == LAMBDA ) {
					name = key ;
					return true ;
				} // if
			} // for
			for ( auto pair : symbolTable ) {
				key = pair.first ;
				if ( name == "#<procedure " + key + ">" && pair.second->type == LAMBDA ) {
					name = key ;
					return true ;
				} // if
			} // for
			
			return false ;
		} // IsLambdaFunction()

		shared_ptr<ASTNode> EvalCons( shared_ptr<ASTNode> node, bool& isDefine, bool& isVerbose, bool isTopLevel, map< string, shared_ptr<ASTNode> > & localSymbolTable ) {
			
		    shared_ptr<ConsNode> cons = static_pointer_cast<ConsNode>(node);
		    shared_ptr<ASTNode> rest = cons->right ;
		    DetectRest( rest, node ) ;
		    shared_ptr<ASTNode> funcNode = nullptr ;
		    try {	
				funcNode = Eval(cons->left, isDefine, isVerbose, false, localSymbolTable);
		    } catch ( NoReturnValueException& e ) {
		    	if ( !e.GetClassified() ) 
		    		throw NoReturnValueException(e.GetNode(), true) ;
				else
					throw ; 
			} // catch
			if ( funcNode->type == CONS ) {
			    throw AttemptToApplyNonFunctionException( funcNode ) ;
			} // if
			else if ( funcNode->type == LAMBDA ) {
			    shared_ptr<LambdaNode> lambda = static_pointer_cast<LambdaNode>(funcNode) ;
			    try {
				    return EvalLambdaCall( rest, lambda, node, localSymbolTable ) ; // ( parameter, lambda node )
			    } catch ( NoReturnValueException& e ) {	
			    	if ( !e.GetClassified() && isTopLevel == true ) 
			    		throw NoReturnValueException(node) ;
					else
						throw ; 
				} // catch
			} // else if
				
			shared_ptr<ATOMNode> func = static_pointer_cast<ATOMNode>(funcNode);
			string funcName = func->value ;
			try {
				if ( funcName == "#<procedure quote>" || funcName == "'" ) {
				    return EvalQuote( rest, node ) ;
				} else if ( funcName == "#<procedure if>" ) {
			        return EvalIF( rest, node, localSymbolTable ) ;
			    } else if ( funcName == "#<procedure cond>" ) {
			        return EvalCond(rest, node, localSymbolTable) ;
			    } else if ( funcName == "#<procedure define>" ) {
			        if ( !isTopLevel ) {
				        throw LevelOfDefineException() ;
				    }
				    isDefine = true ;
				    return EvalDefine( rest, node, localSymbolTable );
				} else if ( funcName == "#<procedure clean-environment>" ) {
					if ( !isTopLevel ) { 
				        throw LevelOfCleanException() ;
				    }
				    isVerbose = true ;
				    return EvalCleanEnvironment( rest, node ) ;
				} else if ( funcName == "#<procedure begin>" ) {
				    return EvalBegin( rest, node, localSymbolTable );
				} else if ( IsBuiltInFunction(funcName) ) {
				    return EvalBuiltIn( funcName, rest, node, localSymbolTable ) ;
				} else if ( funcName == "#<procedure exit>" ) {
				    if ( !isTopLevel ) {
				    	throw LevelOfExitException() ;
				    }
				    EvalExit( rest, node ) ;
				} else if ( funcName == "#<procedure let>" ) {
					return EvalLet( rest, node, localSymbolTable ) ;
				} else if ( funcName == "#<procedure lambda>") {
					return EvalLambdaFunc( rest, node ) ; 
				} else if ( IsLambdaFunction(funcName, localSymbolTable) ) {
					shared_ptr<LambdaNode> lambda ; // = static_pointer_cast<LambdaNode>(funcNode)
					if ( localSymbolTable.count(funcName) )
						lambda = static_pointer_cast<LambdaNode>(CloneNode(localSymbolTable[funcName])) ;
					else if ( symbolTable.count(funcName) )
						lambda = static_pointer_cast<LambdaNode>(CloneNode(symbolTable[funcName])) ;
		
					return EvalLambdaCall( rest, lambda, node, localSymbolTable ) ; // ( parameter, lambda node )
				} else if ( funcName == "#<procedure create-error-object>" ) {
					return EvalCreateErrorObj( rest, node, localSymbolTable ) ;
				} else if ( funcName == "#<procedure error-object?>" ) {
					return EvalIsErrorObj( rest, node, localSymbolTable ) ;
				} else if ( funcName == "#<procedure read>" ) {
					return EvalRead( rest, node, localSymbolTable ) ;
				} else if ( funcName == "#<procedure write>" ) {
					return EvalWrite( rest, node, localSymbolTable ) ;
				} else if ( funcName == "#<procedure display-string>" ) {
					return EvalDisplayString( rest, node, localSymbolTable ) ;
				} else if ( funcName == "#<procedure newline>" ) {
					return EvalNewLine( rest, node, localSymbolTable ) ;
				} else if ( funcName == "#<procedure symbol->string>" ) {
					return EvalSymbolToString( rest, node, localSymbolTable ) ;
				} else if ( funcName == "#<procedure number->string>" ) {
					return EvalNumToString( rest, node, localSymbolTable ) ;
				} else if ( funcName == "#<procedure eval>" ) {
					return Evaluate( rest, node, localSymbolTable ) ;
				} else if ( funcName == "#<procedure set!>" ) {
					return EvalSet( rest, node, localSymbolTable ) ;
				} else if ( funcName == "#<procedure verbose?>" ) {
					return EvalIsVerbose(rest, node, localSymbolTable) ;
				} else if ( funcName == "#<procedure verbose>" ) { 
					return EvalVerbose(rest, node, localSymbolTable) ;
				} else {
				    throw AttemptToApplyNonFunctionException( funcNode ) ;
				}
			} catch ( NoReturnValueException& e ) {	
			    if ( !e.GetClassified() && isTopLevel == true ) 
			    	throw NoReturnValueException(node) ;
				else
					throw ; 
			} // catch
				
		    return nullptr;
		} //EvalCons
		
		shared_ptr<ASTNode> EvalAtom( shared_ptr<ASTNode> node, map< string, shared_ptr<ASTNode> > & localSymbolTable ) {
			
			shared_ptr<ATOMNode> atom = static_pointer_cast<ATOMNode>(node);
			
			if ( IsLiteral( atom ) ) {
				return make_shared<ATOMNode>( atom->value, atom->tokentype );
			} // if IsLiteral
			else if ( IsFunctionName( atom->value ) ) {
				string str = "#<procedure " + atom->value + ">" ;
				return make_shared<ATOMNode>( str, SYMBOL ); // atom->tokentype
			} // if IsFunctionName
			else if ( IsBoundSymbol(atom, localSymbolTable) ) {
				shared_ptr<ASTNode> ans = GetVariable( atom->value, localSymbolTable ) ;
				if ( ans->type == LAMBDA ) {
					shared_ptr<LambdaNode> lambda = static_pointer_cast<LambdaNode>(ans) ;
					return lambda ;
				} // if
				return ans;
			} // else if
			else {
				throw UnboundSymbolException(atom->value) ;
			}
		} // EvalAtom
		
		shared_ptr<ASTNode> EvalLambda( shared_ptr<ASTNode> node ) {
			
			if ( node->type == LAMBDA ) {
				shared_ptr<LambdaNode> lambda = static_pointer_cast<LambdaNode>(node) ;
				return lambda ;	
			} // if
			else {
				throw runtime_error("ERROR IN EvalLambda！") ;
			}
		} // EvalEvalLambda
		
	public:
		shared_ptr<ASTNode> Eval( shared_ptr<ASTNode> node, bool & isDefine, bool & isVerbose, bool isTopLevel, map< string, shared_ptr<ASTNode> > & localSymbolTable ) {
			
			if( !node )
				throw runtime_error("nullptr in Eval！") ;
			
			if( node->type == ATOM ) {
				return EvalAtom( node, localSymbolTable ) ;
			} // if
			else if ( node->type == CONS ) {
				return EvalCons( node, isDefine, isVerbose, isTopLevel, localSymbolTable ) ;
			} // else if
			else if ( node->type == LAMBDA ) {
				return EvalLambda(node) ;
			}
			else {
				throw runtime_error("Unknown node type") ;
			} // else 
			
		} // Eval
	
		bool GetVerbose() {
			return Verbose ;
		} // GetVerbose()
}; 



int main() {
	char c ;
	cin >> gTestNum; // 讀取gTestNum 
	cin.get(c) ; // 讀取gTestNum 後面的\n
	
	cout << "Welcome to OurScheme!\n" ;
	
	// 讀取第一個字元 
	if (!GetNextChar( gNextChar, gNextCharLine, gNextCharColumn )) {
		cout << "ERROR (no more input) : END-OF-FILE encountered\n" ;
		cout << "Thanks for using OurScheme!" ;
		return 0 ;
	} // if

	
	Evaluator evaluator ;
	bool first = true ;
	bool stop = false ;
	string stopMag = "" ;
	shared_ptr<ASTNode> NoMoreInputAST = nullptr ;
	
	while ( true ) {
		
		shared_ptr<ASTNode> AST = nullptr;
		
		try {
			
		    AST = g_parser.parse();
		    cout << "\n> " ;
			if ( IsExit( AST ) ) {
				break;  // 正確退出迴圈
			} // IsExit()

		} // try
		catch ( const NoMoreInputException& e ) {

			stop = true ;
			stopMag = e.what() ;
			NoMoreInputAST = e.GetNode() ;
			
		} catch ( const UnExpectedTokenException& e ) {
			// Line and column !!!!!!!!!!!!!!!
			cout << "\n> " ;
			cout << e.what() << " atom or '(' expected when token at Line " 
          		 << e.getLine() << " Column " << e.getColumn() 
         		 << " is >>" << e.getStr() << "<<\n";
				  	 
			continue ;	
		} catch ( const NoClosingQuoteException& e ){
			cout << "\n> " ;
			cout << e.what() << " END-OF-LINE encountered at Line " 
          		 << e.getLine() << " Column " << e.getColumn() << "\n";
          		 
			continue ;	
		} catch ( const MissingRightParenException& e ) {
			// Line and column !!!!!!!!!!!!!!!
			cout << "\n> " ;
			cout << e.what() << " ')' expected when token at Line " 
          	     << e.getLine() << " Column " << e.getColumn() 
          		 << " is >>" << e.getStr() << "<<\n";
          		 
			continue ;	
		} catch ( const ExitException& e ) {
			break ;
		} 
	
		// 第一次的格式處理 
		if (first) 
			first = false ;
		

		// evalExp
		
		shared_ptr<ASTNode> result = nullptr ;
		bool isDefine = false ;
		bool isVerboseFunc = false ;
		
		try{
			
			map<string, shared_ptr<ASTNode>> LST; // LocalSymbolTable
			
			if ( !stop )
				result = evaluator.Eval(AST,isDefine,isVerboseFunc,true,LST) ; // 傳入AST tree以及空的local map
			else {
				if ( NoMoreInputAST ){
					evaluator.Eval(NoMoreInputAST,isDefine,isVerboseFunc,true,LST) ;
				}
					
			}
			if( result ) {
				printAST( result,false,false,0,true,true ) ;
			}

		} 
		catch ( const runtime_error & e ) {
			cout << e.what() ;
		} catch ( const WrongNumberOfArgumentException & e ) {
			cout << e.what() << "\n" ; 
		} catch ( const NonListException & e ) {
			cout << e.what() ;
			printAST(e.GetNode(),false,false,0,true,true) ;
		} catch ( const DefineFormatException & e ) {
			cout << e.what() ;
			printAST(AST,false,false,0,true,true) ;
		} catch ( const LevelOfDefineException & e ) {
			cout << e.what() << "\n" ;
		} catch ( const NoReturnValueException & e ) {
			cout << e.what() ;
			printAST(e.GetNode(),false,false,0,true,true) ;
		} catch ( const LevelOfCleanException& e ) {
			cout << e.what() << "\n" ;
		} catch ( const UnboundSymbolException& e ) {
			cout << e.what() << "\n" ;
		} catch ( const IncorrectArgumentTypeException& e ) {
			cout << e.what() ;
			printAST(e.getValue(),false,false,0,true,true) ;
		} catch ( const DivisionByZeroException& e ) {
			cout << e.what() << "\n" ;
		} catch ( const AttemptToApplyNonFunctionException& e ) {
			cout << e.what() ;
			printAST(e.getNfunc(),false,false,0,true,true) ;
		} catch ( const CondFormatException& e ) {
			cout << e.what() ;
			printAST(e.GetNode(),false,false,0,true,true) ;
		} catch ( const ExitException& e ) {
			break ;
		} catch ( const LevelOfExitException& e ) {
			cout << e.what() << "\n" ;
		} catch ( const LetFormatException& e ) {
			cout << e.what() ;
			printAST(e.GetNode(),false,false,0,true,true) ;
		} catch ( const LambdaFormatException& e ) {
			cout << e.what() ;
			printAST(e.GetNode(),false,false,0,true,true) ;
		} catch ( const UnboundTestConditionException& e ) {
			cout << e.what() ;
			printAST(e.GetNode(),false,false,0,true,true) ;
		} catch ( const UnboundParameterException& e ) {
			cout << e.what() ;
			printAST(e.GetNode(),false,false,0,true,true) ;
		} catch ( const UnboundConditionException& e ) {
			cout << e.what() ;
			printAST(e.GetNode(),false,false,0,true,true) ;
		} catch ( const SetFormatException& e) {
			cout << e.what() ;
			printAST(e.GetNode(),false,false,0,true,true) ;
		}
		
		
		
		
		if (stop) {
		    cout << "\n> " ;
			cout << stopMag  ;
			break ;
		}
			
	} // while
	
	cout << "\nThanks for using OurScheme!" ;
} // main


// NOTE:   // Project 4
//  create-error-object  ( done )
//  error-object?        ( done )
//  read                 ( done )
//  write                ( done ) 
//  display-string       ( done )
//  newline              ( done )
//  symbol->string       ( done ) 
//  number->string       ( done ) 
//  eval                 ( done ) 
//  set!                 ( need to do )
//  verbose?             ( done ) 
//  verbose              ( done )
// deal with ; "go for it ( 有問題! )
