#include "lk_vm.h"

namespace lk {

OpCodeEntry op_table[] = {
	{ ADD, "add" }, // impl
	{ SUB, "sub" }, // impl
	{ MUL, "mul" }, // impl
	{ DIV, "div" }, // impl
	{ LT, "lt" }, // impl
	{ GT, "gt" }, // impl
	{ LE, "le" }, // impl
	{ GE, "ge" }, // impl
	{ NE, "ne" }, // impl
	{ EQ, "eq" }, // impl
	{ INC, "inc" }, // impl
	{ DEC, "dec" }, // impl
	{ OR, "or" }, // impl
	{ AND, "and" }, // impl
	{ NOT, "not" }, // impl
	{ NEG, "neg" }, // impl
	{ EXP, "exp" }, // impl
	{ PSH, "psh" }, // impl
	{ POP, "pop" }, // impl
	{ NUL, "nul" }, // impl
	{ DUP, "dup", }, // impl
	{ ARG, "arg" }, // impl
	{ J, "j" }, // impl
	{ JF, "jf" }, // impl
	{ JT, "jt" }, // impl
	{ IDX, "idx" }, // impl
	{ KEY, "key" }, // impl
	{ MAT, "mat" }, // impl
	{ WAT, "wat" }, // impl
	{ SET, "set" }, // impl
	{ GET, "get" }, // impl
	{ WR, "wr" }, // impl
	{ RREF, "rref" }, // impl
	{ NREF, "nref" }, // impl
	{ CREF, "cref" }, // impl
	{ FREF, "fref" }, // impl
	{ CALL, "call" }, // impl
	{ TCALL, "tcall" }, // impl
	{ RET, "ret" }, // impl
	{ END, "end" }, // impl
	{ SZ, "sz" }, // impl
	{ KEYS, "keys" }, // impl
	{ TYP, "typ" }, // impl
	{ VEC, "vec" },
	{ HASH, "hash" },
	{ __MaxOp, 0 } };

#ifdef OP_PROFILE
void vm::clear_opcount() {
	for( size_t i=0;i<__MaxOp;i++ )
		opcount[i] = 0;
}
#endif

vm::vm( size_t ssize )
{
	ip = sp = 0;
	stack.resize( ssize, vardata_t() );
	frames.reserve( 16 );

#ifdef OP_PROFILE
	clear_opcount();
#endif

}

#ifdef OP_PROFILE
void vm::get_opcount( size_t iop[__MaxOp] )
{
	for( size_t i=0;i<__MaxOp;i++ )
		iop[i] = opcount[i];
}
#endif

vm::~vm()
{
	free_frames();
}

void vm::free_frames()
{
	for( size_t i=0;i<frames.size(); i++ )
		delete frames[i];
	frames.clear();
}

vm::frame **vm::get_frames( size_t *nfrm ) {
	*nfrm = frames.size();
	if ( frames.size() > 0 ) return &frames[0];
	else return 0;
}

vardata_t *vm::get_stack( size_t *psp ) {
	*psp = sp;
	return &stack[0];
}

void vm::load( const std::vector<unsigned int> &code,
	const std::vector<vardata_t> &cnstvals,
	const std::vector<lk_string> &ids,
	const std::vector<lk::srcpos_t> &dbginf)
{
	program = code;
	constants = cnstvals;
	identifiers = ids;
	debuginfo = dbginf;

	free_frames();
}
	
bool vm::special_set( const lk_string &name, vardata_t &val )
{
	throw error_t( "no defined mechanism to set special variable '" + name + "'" );
}

bool vm::special_get( const lk_string &name, vardata_t &val )
{
	throw error_t( "no defined mechanism to get special variable '" + name + "'" );
}

void vm::initialize( lk::env_t *env )
{
#ifdef OP_PROFILE
	clear_opcount();
#endif

	free_frames();
	errStr.clear();

	ip = sp = 0;
	for( size_t i=0;i<stack.size();i++ )
		stack[i].nullify();
		
	frames.push_back( new frame( env, 0, 0, 0 ) );

	ibrkln = 0;
}

enum ExecMode { NORMAL, DEBUG, SINGLE };

#define CHECK_FOR_ARGS(n) if ( sp < n ) return error("stack [sp=%d] error, %d arguments required", sp, n );
#define CHECK_OVERFLOW() if ( sp >= stack.size() ) return error("stack overflow [sp=%d]", stack.size())
#define CHECK_CONSTANT() if ( arg >= constants.size() ) return error( "invalid constant value address: %d\n", arg )
#define CHECK_IDENTIFIER() if ( arg >= identifiers.size() ) return error( "invalid identifier address: %d\n", arg )

bool vm::run( ExecMode mode )
{
	if( frames.size() == 0 ) return error("vm not initialized"); // must initialize first.

	vardata_t nullval;
	size_t nexecuted = 0;
	const size_t code_size = program.size();
	size_t next_ip = code_size;
	vardata_t *lhs, *rhs;
	try {
		while ( ip < code_size )
		{
			Opcode op = (Opcode)(unsigned char)program[ip];
			size_t arg = ( program[ip] >> 8 );

#ifdef OP_PROFILE
			opcount[op]++;
#endif

			if ( mode == DEBUG )
			{
				if ( ip < debuginfo.size() && debuginfo[ip].line == (int)ibrkln )
					return true;
			}

			next_ip = ip+1;
			
			rhs = ( sp >= 1 ) ? &stack[sp-1] : NULL;
			lhs = ( sp >= 2 ) ? &stack[sp-2] : NULL;

			vardata_t &rhs_deref( rhs ? rhs->deref() : nullval );
			vardata_t &lhs_deref( lhs ? lhs->deref() : nullval );
			vardata_t &result( lhs ? *lhs : *rhs );

			switch( op )
			{
			case RREF:
			case CREF:
			case NREF:
			{
				frame &F = *frames.back();
				CHECK_OVERFLOW();
				CHECK_IDENTIFIER();

				if ( fcallinfo_t *fci = F.env.lookup_func( identifiers[arg] ) )
				{
					stack[sp++].assign_fcall( fci );
				}
				else if ( vardata_t *x = F.env.lookup( identifiers[arg], op == RREF ) )
				{
					stack[sp++].assign( x );
				}
				else if ( op == CREF || op == NREF )
				{
					vardata_t *x = new vardata_t;
					if ( op == CREF )
					{
						x->set_flag( vardata_t::CONSTVAL );
						x->clear_flag( vardata_t::ASSIGNED );
					}
					F.env.assign( identifiers[arg], x );
					stack[sp++].assign( x );
				}
				else
					return error("referencing unassigned variable: %s\n", (const char*)identifiers[arg].c_str() );

				break;
			}
			
			case CALL:
			case TCALL:
			{
				CHECK_FOR_ARGS( arg+2 );
				if ( vardata_t::EXTFUNC == rhs_deref.type() && op == CALL )
				{
					frame &F = *frames.back();
					fcallinfo_t *fci = rhs_deref.fcall();
					vardata_t &retval = stack[ sp - arg - 2 ];
					invoke_t cxt( &F.env, retval, fci->user_data );

					for( size_t i=0;i<arg;i++ )
						cxt.arg_list().push_back( stack[sp-arg-1+i] );						
							
					try {
						if ( fci->f ) (*(fci->f))( cxt );
						else if ( fci->f_ext ) lk::external_call( fci->f_ext, cxt );
						else cxt.error( "invalid internal reference to function" );

						sp -= (arg+1); // leave return value on stack (even if null)
					}
					catch( std::exception &e )
					{
						return error( e.what() );
					}
				}
				else if ( vardata_t::INTFUNC == rhs_deref.type() )
				{
					frames.push_back( new frame( &frames.back()->env, sp, next_ip, arg ) );
					frame &F = *frames.back();
												
					vardata_t *__args = new vardata_t;
					__args->empty_vector();

					size_t offset = 1;						
					if ( op == TCALL )
					{
						offset = 2;
						F.env.assign( "this", new vardata_t( stack[sp-2] ) );
						F.thiscall = true;
					}

					for( size_t i=0;i<arg;i++ )
						__args->vec()->push_back( stack[sp-arg-offset+i] );		
						
					F.env.assign( "__args", __args );

					next_ip = rhs_deref.faddr(); 
				}
				else
					return error("invalid function access");
			}
				break;

			case ARG:
				if ( frames.size() > 0 )
				{
					frame &F = *frames.back();
					if ( F.iarg >= F.nargs )
						return error("too few arguments passed to function");

					size_t offset = F.thiscall ? 2 : 1;
					size_t idx = F.fp - F.nargs - offset + F.iarg;

					vardata_t *x = new vardata_t;
					x->assign( &stack[idx] );
					F.env.assign( identifiers[arg], x );
					F.iarg++;
				}
				break;

			case PSH:
				CHECK_OVERFLOW();
				CHECK_CONSTANT();
				stack[sp++].copy( constants[arg] );
				break;
			case POP:
				if ( sp == 0 ) return error("stack corruption at level 0");
				sp--;
				break;
			case J:
				next_ip = arg;
				break;
			case JT:
				CHECK_FOR_ARGS( 1 );
				if ( rhs_deref.as_boolean() ) next_ip = arg;
				sp--;
				break;
			case JF:
				CHECK_FOR_ARGS( 1 );
				if ( !rhs_deref.as_boolean() ) next_ip = arg;
				sp--;
				break;
			case IDX:
				{
					CHECK_FOR_ARGS( 2 );
					size_t index = rhs_deref.as_unsigned();
					vardata_t &arr = lhs_deref;
					bool is_mutable = ( arg != 0 );
					if ( is_mutable &&
						( arr.type() != vardata_t::VECTOR
						|| arr.length() <= index ) )
						arr.resize( index + 1 );

					result.assign( arr.index(index) );
					sp--;
				}
				break;
			case KEY:
				{
					CHECK_FOR_ARGS( 2 );
					lk_string key( rhs_deref.as_string() );
					vardata_t &hash = lhs_deref;
					bool is_mutable = (arg != 0);
					if ( is_mutable && hash.type() != vardata_t::HASH )
						hash.empty_hash();

					vardata_t *x = hash.lookup( key );
					if ( !x ) hash.assign( key, x=new vardata_t );

					result.assign( x );
					sp--;
				}
				break;

			case ADD:
				CHECK_FOR_ARGS( 2 );
				if ( lhs_deref.type() == vardata_t::STRING || rhs_deref.type() == vardata_t::STRING )
					result.assign( lhs_deref.as_string() + rhs_deref.as_string() );
				else
					result.assign( lhs_deref.num() + rhs_deref.num() );
				sp--;
				break;
			case SUB:
				CHECK_FOR_ARGS( 2 );
				result.assign( lhs_deref.num() - rhs_deref.num() );
				sp--;
				break;
			case MUL:
				CHECK_FOR_ARGS( 2 );
				result.assign( lhs_deref.num() * rhs_deref.num() );
				sp--;
				break;
			case DIV:
				CHECK_FOR_ARGS( 2 );
				if ( rhs_deref.num() == 0.0 )
					result.assign( std::numeric_limits<double>::quiet_NaN() );
				else
					result.assign( lhs_deref.num() / rhs_deref.num() );
				sp--;
				break;
			case EXP:
				CHECK_FOR_ARGS( 2 );
				result.assign( ::pow( lhs_deref.num() , rhs_deref.num() ) );
				sp--;
				break;
			case LT:
				CHECK_FOR_ARGS( 2 );
				result.assign( lhs_deref.lessthan( rhs_deref ) ? 1.0 : 0.0 );
				sp--;
				break;
			case LE:
				CHECK_FOR_ARGS( 2 );
				result.assign( ( lhs_deref.lessthan( rhs_deref )
					|| lhs_deref.equals( rhs_deref ) ) ? 1.0 : 0.0 );
				sp--;
				break;
			case GT:
				CHECK_FOR_ARGS( 2 );
				result.assign( ( !lhs_deref.lessthan( rhs_deref )
					&& !lhs_deref.equals( rhs_deref ) )  ? 1.0 : 0.0  );
				sp--;
				break;
			case GE:
				CHECK_FOR_ARGS( 2 );
				result.assign( !( lhs_deref.lessthan( rhs_deref ))  ? 1.0 : 0.0  );
				sp--;
				break;
			case EQ:
				CHECK_FOR_ARGS( 2 );
				result.assign(  lhs_deref.equals( rhs_deref )  ? 1.0 : 0.0  );
				sp--;
				break;
			case NE:
				CHECK_FOR_ARGS( 2 );
				result.assign(  lhs_deref.equals( rhs_deref )  ? 0.0 : 1.0  );
				sp--;
				break;
			case OR:
				CHECK_FOR_ARGS( 2 );
				result.assign(  (((int)lhs_deref.num()) || ((int)rhs_deref.num() )) ? 1 : 0   );
				sp--;
				break;
			case AND:
				CHECK_FOR_ARGS( 2 );
				result.assign(  (((int)lhs_deref.num()) && ((int)rhs_deref.num() )) ? 1 : 0   );
				sp--;
				break;
			case INC:
				CHECK_FOR_ARGS( 1 );
				rhs_deref.assign( rhs_deref.num() + 1.0 );
				//result.copy( *rhs );
				break;
			case DEC:
				CHECK_FOR_ARGS( 1 );
				rhs_deref.assign( rhs_deref.num() - 1.0 );
				//result.copy( *rhs );
				break;
			case NOT:
				CHECK_FOR_ARGS( 1 );
				result.assign( ((int)rhs_deref.num()) ? 0 : 1 );
				break;
			case NEG:
				CHECK_FOR_ARGS( 1 );
				result.assign( 0.0 - rhs_deref.num() );
				break;
			case MAT:
				CHECK_FOR_ARGS( 2 );
				if ( lhs_deref.type() == vardata_t::HASH )
				{
					lk::varhash_t *hh = lhs_deref.hash();
					lk::varhash_t::iterator it = hh->find( rhs_deref.as_string() );
					if ( it != hh->end() )
						hh->erase( it );
				}
				else if( lhs_deref.type() == vardata_t::VECTOR )
				{
					std::vector<lk::vardata_t> *vv = lhs_deref.vec();
					size_t idx = rhs_deref.as_unsigned();
					if ( idx < vv->size() )
						vv->erase( vv->begin() + idx );
				}
				else
					return error( "-@ requires a hash or vector" );

				sp--;
				break;

			case WAT:
				if ( lhs_deref.type() == vardata_t::HASH )
				{
					lk::varhash_t *hh = lhs_deref.hash();
					result.assign( hh->find( rhs_deref.as_string() ) != hh->end() ? 1.0 : 0.0 );
				}
				else if ( lhs_deref.type() == vardata_t::VECTOR )
				{
					result.assign( -1.0 );
					std::vector<lk::vardata_t> *vv = lhs_deref.vec();
					for( size_t i=0;i<vv->size();i++ )
					{
						if ( (*vv)[i].equals( rhs_deref ) )
						{
							result.assign( (double)i );
							break;
						}
					}
				}
				else if ( lhs_deref.type() == vardata_t::STRING )
				{
					lk_string::size_type pos = lhs_deref.str().find( rhs_deref.as_string() );
					result.assign( pos!=lk_string::npos ? (int)pos : -1.0 );
				}
				else
					return error("?@ requires a hash, vector, or string");
				
				sp--;
				break;

			case GET:
				CHECK_OVERFLOW();
				CHECK_IDENTIFIER();
				if ( !special_get( identifiers[arg], stack[sp++] ) )
					return error("failed to read external value '%s'", (const char*)identifiers[arg].c_str() );
				break;

			case SET:
				CHECK_FOR_ARGS( 1 );
				CHECK_IDENTIFIER();
				if ( !special_set( identifiers[arg], rhs_deref ) )
					return error("failed to write external value '%s'", (const char*)identifiers[arg].c_str() );
				sp--;
				break;
			case SZ:
				CHECK_FOR_ARGS( 1 );
				if (rhs_deref.type() == vardata_t::VECTOR)
					rhs->assign( (int) rhs_deref.length() );
				else if (rhs_deref.type() == vardata_t::STRING)
					rhs->assign( (int) rhs_deref.str().length() );
				else if (rhs_deref.type() == vardata_t::HASH)
				{
					int count = 0;

					varhash_t *h = rhs_deref.hash();
					for( varhash_t::iterator it = h->begin();
						it != h->end();
						++it )
					{
						if ( (*it).second->deref().type() != vardata_t::NULLVAL )
							count++;
					}
					rhs->assign( count );
				}
				else
					return error( "operand to sizeof must be a array, string, or table type");

				break;
			case KEYS:
				CHECK_FOR_ARGS( 1 );
				if (rhs_deref.type() == vardata_t::HASH)
				{
					varhash_t *h = rhs_deref.hash();
					result.empty_vector();
					result.vec()->reserve( h->size() );
					for( varhash_t::iterator it = h->begin();
						it != h->end();
						++it )
					{
						if ( (*it).second->deref().type() != vardata_t::NULLVAL )
							result.vec_append( (*it).first );
					}
					return true;
				}
				else
					return error( "operand to @ (keysof) must be a table");
				sp--;
				break;
			case WR:
				CHECK_FOR_ARGS( 2 );
				rhs_deref.copy( lhs_deref );
				sp--;
				break;

			case TYP:
				CHECK_OVERFLOW();
				CHECK_IDENTIFIER();

				if ( vardata_t *x = frames.back()->env.lookup( identifiers[arg], true ) )
					stack[sp++].assign( x->typestr() );
				else
					stack[sp++].assign( "unknown" );
				break;

			case FREF:
				CHECK_OVERFLOW();
				stack[sp++].assign_faddr( arg );
				break;
				

			case RET:
				if ( frames.size() > 1 )
				{
						
					vardata_t *result = &stack[sp-1];
					frame &F = *frames.back();
					size_t ncleanup = F.nargs + 1 + arg;
					if ( F.thiscall ) ncleanup++;

					if ( sp <= ncleanup ) 
						return error("stack corruption upon function return (sp=%d, nc=%d)", (int)sp, (int)ncleanup);
					sp -= ncleanup;
					stack[sp-1].copy( result->deref() );
					next_ip = F.retaddr;

					delete frames.back();
					frames.pop_back();
				}
				else
					next_ip = code_size;

				break;

			case END:
				next_ip = code_size;
				break;

			case NUL:
				CHECK_OVERFLOW();
				stack[sp].nullify();
				sp++;
				break;

			case DUP:
				CHECK_OVERFLOW();
				CHECK_FOR_ARGS( 1 );
				stack[sp].copy( stack[sp-1] );
				sp++;
				break;

			case VEC:
			{
				CHECK_FOR_ARGS( arg );
				if ( arg > 0 )
				{
					vardata_t &vv = stack[sp-arg];
					vardata_t save1;
					save1.copy( vv.deref() );
					vv.empty_vector();
					vv.vec()->resize( arg );
					vv.index(0)->copy( save1 );
					for( size_t i=1;i<arg;i++ )
						vv.index(i)->copy( stack[sp-arg+i].deref() );
					sp -= (arg-1);
				}
				else
				{
					CHECK_OVERFLOW();
					stack[sp].empty_vector();
					sp++;
				}
				break;
			}
			case HASH:
			{
				size_t N = arg*2;
				CHECK_FOR_ARGS( N );
				vardata_t &vv = stack[sp-N];
				lk_string key1( vv.deref().as_string() );
				vv.empty_hash();
				if ( arg > 0 )
				{
					for( size_t i=0;i<N;i+=2 )
						vv.hash_item( i==0 ? key1 :
							stack[sp-N+i].as_string() ).copy( 
							stack[sp-N+i+1].deref() );
				}
				sp -= (N-1);
				break;
			}
					
			default:
				return error( "invalid instruction (0x%02X)", (unsigned int)op );
			};

			ip = next_ip;

			nexecuted++;
			if ( mode == SINGLE && nexecuted > 0 ) return true;
		}
	} catch( std::exception &exc ) {
		return error("runtime exception @ %d: %s", (int)ip, exc.what() );
	}

	return true;
}
	
bool vm::error( const char *fmt, ... )
{
	char buf[512];
	va_list args;
	va_start( args, fmt );
	vsprintf( buf, fmt, args );
	va_end( args );
	errStr = buf;	
	return false;
}

int vm::setbrk( int line )
{
	for( size_t i=0;i<debuginfo.size();i++ ) {
		if ( debuginfo[i].line >= line ) {
			ibrkln = debuginfo[i].line;
			return debuginfo[i].line;
		}
	}

	return -1;
}

} // namespace lk;
