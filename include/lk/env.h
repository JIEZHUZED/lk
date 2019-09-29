/***********************************************************************************************************************
*  LK, Copyright (c) 2008-2017, Alliance for Sustainable Energy, LLC. All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
*  following conditions are met:
*
*  (1) Redistributions of source code must retain the above copyright notice, this list of conditions and the following
*  disclaimer.
*
*  (2) Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
*  following disclaimer in the documentation and/or other materials provided with the distribution.
*
*  (3) Neither the name of the copyright holder nor the names of any contributors may be used to endorse or promote
*  products derived from this software without specific prior written permission from the respective party.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
*  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
*  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER, THE UNITED STATES GOVERNMENT, OR ANY CONTRIBUTORS BE LIABLE FOR
*  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
*  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
*  AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
*  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********************************************************************************************************************/

#ifndef __lk_var_h
#define __lk_var_h

#include <vector>
#include <cstdio>
#include <cstdarg>
#include <exception>

#include <lk/absyn.h>
#include <lk/invoke.h>

/// create and associate a doc_t from within cxt, an invoke_t, if cxt doesn't yet have one
#define LK_DOC(  fn, desc, sig ) if (cxt.doc_mode()) { cxt.document( lk::doc_t(fn , "", desc, sig ) ); return; }
#define LK_DOC1( fn, notes, desc1, sig1 ) if (cxt.doc_mode()) { cxt.document( lk::doc_t(fn , notes, desc1, sig1 )); return; }
#define LK_DOC2( fn, notes, desc1, sig1, desc2, sig2 ) if (cxt.doc_mode()) { cxt.document( lk::doc_t(fn , notes, desc1, sig1, desc2, sig2 )); return; }
#define LK_DOC3( fn, notes, desc1, sig1, desc2, sig2, desc3, sig3 ) if (cxt.doc_mode()) { cxt.document( lk::doc_t(fn , notes, desc1, sig1, desc2, sig2, desc3, sig3 )); return; }


namespace lk {
	class vardata_t;
	struct fcallinfo_t;
	struct bytecode;
	typedef unordered_map< lk_string, vardata_t*, lk_string_hash, lk_string_equal > varhash_t;
	
/**
* \class error_t
*
* 
*
*
*/
	
	class error_t : public std::exception
	{
	public:
		error_t() : text("general data exception") {  }
		error_t(const lk_string &s) : text(s) {  }
		error_t(const char *fmt, ...) {
			char buf[512];
			va_list args;
			va_start(args, fmt);
#ifdef _WIN32
			_vsnprintf(buf, 511, fmt, args);
#else
			vsnprintf(buf, 511, fmt, args);
#endif
			va_end(args);
			text = buf;
		}
		//error_t(const lk_string &t) : text(t) {  }
		virtual ~error_t() throw() {  }
		lk_string text;
		virtual const char *what() const throw (){ return text.c_str(); }
	};

/**
* \class vardata_t
*
* Vardata_t form the execution stack of the vm: stores identifiers and expressions, 
* arguments and results for function invocations, and operations.
*/

	class vardata_t
	{
	private:
		unsigned char m_type;
		/** \union m_u
		* 
		* m_type stores both data type and flag information. 
		* Lower four bits are data type (16 possible values)
		* Upper four bits are boolean flags (CONSTVAL, etc)
		*/
		union {
			void *p;
			double v;
		} m_u;

		void set_type( unsigned char ty );
		void assert_modify();
		
	public:
		/// Data Types
		static const unsigned char NULLVAL = 1;
		static const unsigned char REFERENCE = 2;
		static const unsigned char NUMBER = 3;
		static const unsigned char STRING = 4;
		static const unsigned char VECTOR = 5;
		static const unsigned char HASH = 6;
		static const unsigned char FUNCTION = 7;	///< code expression pointer
		static const unsigned char EXTFUNC = 8;		///< external function pointer
		static const unsigned char INTFUNC = 9;		///< internal function pointer

		static const unsigned char TYPEMASK = 0x0F;
		static const unsigned char FLAGMASK = 0xF0;

		/// Flags
		static const unsigned char ASSIGNED = 1;
		static const unsigned char CONSTVAL = 2;
		static const unsigned char GLOBALVAL = 3;

		vardata_t();
		vardata_t(const vardata_t &cp);
		~vardata_t();

		inline unsigned char type() const { return (m_type & TYPEMASK); }
		const char *typestr() const;

		void set_flag(unsigned char flag) { m_type |= (0x01 << flag) << 4; }
		void clear_flag(unsigned char flag) { m_type &= ~((0x01 << flag) << 4); }
		bool flagval(unsigned char flag) const { return ((m_type >> flag) >> 4) & 0x01; }

		bool as_boolean() const;
		unsigned int as_unsigned() const;
		int as_integer() const;
		lk_string as_string() const;
		double as_number() const;

		bool equals( vardata_t &rhs ) const;
		bool lessthan( vardata_t &rhs ) const;

		void nullify(); ///< only function that override const-ness
		
		void deep_localize();

		bool copy(vardata_t &rhs);
		vardata_t &operator=(const vardata_t &rhs)
		{
			copy(const_cast<vardata_t&>(rhs));
			return *this;
		}
		/// return referenced vardata_t
		inline vardata_t &deref() const {
			vardata_t *p = const_cast<vardata_t*>(this);
			while (p->type() == REFERENCE) {
				vardata_t *pref = reinterpret_cast<vardata_t*>(p->m_u.p);
				if (p == pref) throw error_t("self referential reference");
				p = pref;
			}
			if (!p) throw error_t("dereference resulted in null target");
			return *p;
		}
				
		void assign( double d );
		void assign( const char *s );
		void assign( const lk_string &s );
		void empty_vector();
		void empty_hash();
		void assign( const lk_string &key, vardata_t *val );
		void unassign( const lk_string &key );
		void assign( expr_t *func ); ///< does NOT take ownership (expr_t must be deleted by the environment
		void assign( vardata_t *ref ); ///< makes this vardata_t a reference to the object 'ref'

		void assign_fcall( fcallinfo_t *fci );
		void assign_faddr( size_t bcip );

		void resize( size_t n );

		vardata_t *ref() const;
		double num() const;
		lk_string str() const;
		expr_t *func() const;
		vardata_t *index(size_t idx) const; ///< returned variable inherits const-ness of parent
		size_t length() const ;
		vardata_t *lookup( const lk_string &key ) const; ///< returned variable inherits const-ness of parent
		fcallinfo_t *fcall() const;
		size_t faddr() const;

		std::vector<vardata_t> *vec() const;
		void vec_append( double d );
		void vec_append( const lk_string &s );
        void vec_append(const vardata_t vd);

        varhash_t *hash() const;
		void hash_item( const lk_string &key, double d );
		void hash_item( const lk_string &key, const lk_string &s );
		void hash_item( const lk_string &key, const vardata_t &v );
		vardata_t &hash_item( const lk_string &key );

	};
	
	class env_t;

/**
* \class objref_t
*
* 
*
*
*/

	class objref_t
	{
	private:
		friend class env_t;
		env_t *m_env;

	public:
		objref_t() { m_env = 0; };
		virtual ~objref_t() { }
		env_t *get_env() { return m_env; }
		virtual lk_string type_name() = 0;
	};
	
	class invoke_t;

	typedef void (*fcall_t)( lk::invoke_t& );

/**
* \struct fcallinfo_t
*
* links together LK function name f, user data and lk_invokable
*
*
*/
	struct fcallinfo_t {
		fcall_t f;
		lk_invokable f_ext;
		void *user_data;
	};

	typedef unordered_map< lk_string, fcallinfo_t, lk_string_hash, lk_string_equal > funchash_t;


/** Documents LK functions.
 * \class doc_t
 *
 * Records function name, descriptions such as "Input text from user", input:output 
 * sig such as "(none):string", and number of pairs of desc/sig. Used in function window
 * in LK scripting window.
 *
 */
	class doc_t
	{
		friend class invoke_t;
	public:
		doc_t() : has_2(false), has_3(false), m_ok(false) {  }
		doc_t(const char *f, const char *n,
			const char *d1, const char *s1)
			: func_name(f), notes(n),
			desc1(d1), sig1(s1),
			desc2(""), sig2(""),
			desc3(""), sig3(""),
			has_2(false), has_3(false), m_ok(false) { }

		doc_t(const char *f, const char *n,
			const char *d1, const char *s1,
			const char *d2, const char *s2)
			: func_name(f), notes(n),
			desc1(d1), sig1(s1),
			desc2(d2), sig2(s2),
			desc3(""), sig3(""),
			has_2(true), has_3(false), m_ok(false) { }

		doc_t(const char *f, const char *n,
			const char *d1, const char *s1,
			const char *d2, const char *s2,
			const char *d3, const char *s3)
			: func_name(f), notes(n),
			desc1(d1), sig1(s1),
			desc2(d2), sig2(s2),
			desc3(d3), sig3(s3),
			has_2(true), has_3(true), m_ok(false) { }

		lk_string func_name;
		lk_string notes;
		lk_string desc1, sig1;
		lk_string desc2, sig2;
		lk_string desc3, sig3;
		bool has_2, has_3;

		bool ok() { return m_ok; }

		static bool info(fcallinfo_t *f, doc_t &d);
		static bool info(fcall_t f, doc_t &d);
		static bool info(lk_invokable f, doc_t &d);
	private:
		void copy_data(doc_t *p);
		bool m_ok;
	};

/**
 * \class invoke_t
 *
 * Used for input and output to an associated LK function: stores arguments and results in vardata_t and logs errors
 *
 */

	class invoke_t
	{
		friend class doc_t;
	private:
		doc_t *m_docPtr;
		env_t *m_env;
		vardata_t &m_resultVal;
		std::vector< vardata_t > m_argList;

		lk_string m_error;
		bool m_hasError;

		void *m_userData;

		// for threading existing bytecode
		bytecode *m_bc;

	public:
		invoke_t(env_t *e, vardata_t &result, void *user_data = 0, bytecode *bc = 0)
			: m_docPtr(0), m_env(e), m_resultVal(result), m_hasError(false), m_userData(user_data), m_bc(bc) { }

		bool doc_mode();
		void document(doc_t d);
		env_t *env() { return m_env; }
		vardata_t &result() { return m_resultVal; }
		void *user_data() { return m_userData; }
		bytecode *bc() { return m_bc; }


		std::vector< vardata_t > &arg_list() { return m_argList; }
		size_t arg_count() { return m_argList.size(); }

		/// returns the values of user-defined inputs for use as arguments to functions
		vardata_t &arg(size_t idx) {
			if (idx < m_argList.size())	return m_argList[idx].deref();
			else throw error_t( "invalid access to function argument %d, only %d given", idx, m_argList.size());
		}

		void error(const lk_string &text) { m_error = text; m_hasError = true; }
		lk_string error() { return m_error; }
		bool has_error() { return m_hasError; }
		void clear_error() { m_hasError = false; m_error = "";}
	};
	
/**
 * \env_t
 *
 * Stores LK functions in m_funcHash[function name] = fcallinfo
 *
 */

	class env_t
	{
	public:	

/**
* \struct dynlib_t
*
* 
*
*
*/
		struct dynlib_t
		{
			lk_string path;
			void *handle;
			lk_invokable *functions;
		};

	protected:
		env_t *m_parent;

		varhash_t m_varHash;
		varhash_t::iterator m_varIter;

		funchash_t m_funcHash;
		std::vector< objref_t* > m_objTable;

		std::vector< dynlib_t > m_dynlibList;

		bool register_ext_func(lk_invokable f, void *user_data = 0);
		void unregister_ext_func(lk_invokable f);

	public:

		env_t();
		env_t(env_t *p);
		virtual ~env_t();

		void clear_vars();
		void clear_objs();

		void assign(const lk_string &name, vardata_t *value);
		void unassign(const lk_string &name);
		vardata_t *lookup(const lk_string &name, bool search_hierarchy);
		bool first(lk_string &key, vardata_t *&value);
		bool next(lk_string &key, vardata_t *&value);
		unsigned int size();
		void set_parent(env_t *p);
		env_t *parent();
		env_t *global();

		bool register_func(fcall_t f, void *user_data = 0);
		bool register_funcs(std::vector<fcall_t> l, void *user_data = 0);
		bool register_funcs(fcall_t list[], void *user_data = 0); // null item terminated list

		bool load_library(const lk_string &path);
		bool unload_library(const lk_string &path);
		std::vector< dynlib_t* > libraries();

		fcallinfo_t *lookup_func(const lk_string &name);
		std::vector<lk_string> list_funcs();

		size_t insert_object( objref_t *o );
		bool destroy_object( objref_t *o );
		objref_t *query_object( size_t ref );

		void call( const lk_string &name,
				   std::vector< vardata_t > &args,
				   vardata_t &result );
		
	};
	
	/// implemented in lk_invoke.cpp for external dll calls
	void external_call( lk_invokable p, lk::invoke_t &cxt );

}; // namespace lk

#endif
