#ifndef __lk_codegen_h
#define __lk_codegen_h

#include "lk_absyn.h"
#include "lk_vm.h"

namespace lk {

class code_gen
{
public:
	code_gen();
	
	lk_string error() { return m_errStr; }
	
	bool emitasm( lk::node_t *root );

	size_t bytecode( std::vector<unsigned int> &bc, 
		std::vector<vardata_t> &constData, 
		std::vector<lk_string> &idList,
		std::vector<srcpos_t> &debugInfo );

	void textout( lk_string &assembly, lk_string &bytecode );

private:	
	struct instr {
		instr( srcpos_t sp, Opcode _op, int _arg, const char *lbl = 0 )
			: pos(sp), op(_op), arg(_arg) {
			label = 0;
			if ( lbl ) label = new lk_string(lbl);
		}

		instr( const instr &cpy )
		{
			copy( cpy );
		}
		~instr() { 
			if (label) delete label;
		}
		void copy( const instr &cpy )
		{
			pos = cpy.pos;
			op = cpy.op;
			arg = cpy.arg;
			label = 0;
			if ( cpy.label )
				label = new lk_string(*cpy.label);
		}

		instr &operator=( const instr &rhs ) {
			copy( rhs );
			return *this;
		}

		lk::srcpos_t pos;
		Opcode op;
		int arg;
		lk_string *label;
	};

	std::vector<instr> m_asm;
	typedef unordered_map< lk_string, int, lk_string_hash, lk_string_equal > LabelMap;
	LabelMap m_labelAddr;
	std::vector< vardata_t > m_constData;
	std::vector< lk_string > m_idList;
	int m_labelCounter;
	std::vector<lk_string> m_breakAddr, m_continueAddr;
	lk_string m_errStr;

	bool error( const char *fmt, ... );

	int place_identifier( const lk_string &id );
	int place_const( vardata_t &d );
	int const_value( double value );
	int const_literal( const lk_string &lit );
	lk_string new_label();
	void place_label( const lk_string &s );
	int emit( srcpos_t pos, Opcode o, int arg = 0);
	int emit( srcpos_t pos, Opcode o, const lk_string &L );
	bool initialize_const_vec( lk::list_t *v, vardata_t &vvec );
	bool initialize_const_hash( lk::list_t *v, vardata_t &vhash );
	bool pfgen_stmt( lk::node_t *root, unsigned int flags );
	bool pfgen( lk::node_t *root, unsigned int flags );
};

}; // namespace lk

#endif
