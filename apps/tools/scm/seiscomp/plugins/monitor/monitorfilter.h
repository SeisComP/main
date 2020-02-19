/***************************************************************************
 * Copyright (C) GFZ Potsdam                                               *
 * All rights reserved.                                                    *
 *                                                                         *
 * GNU Affero General Public License Usage                                 *
 * This file may be used under the terms of the GNU Affero                 *
 * Public License version 3.0 as published by the Free Software Foundation *
 * and appearing in the file LICENSE included in the packaging of this     *
 * file. Please review the following information to ensure the GNU Affero  *
 * Public License version 3.0 requirements will be met:                    *
 * https://www.gnu.org/licenses/agpl-3.0.html.                             *
 ***************************************************************************/

#ifndef SEISCOMP_APPLICATIONS_MONITORFILTER_H__
#define SEISCOMP_APPLICATIONS_MONITORFILTER_H__

//#define BOOST_SPIRIT_DEBUG
#include <boost/version.hpp>

#if BOOST_VERSION < 103800
#include <boost/spirit.hpp>
#include <boost/spirit/tree/ast.hpp>
namespace bs = boost::spirit;
#else
#include <boost/spirit/include/classic.hpp>
#include <boost/spirit/include/classic_ast.hpp>
namespace bs = boost::spirit::classic;
#endif

#include <seiscomp/core/strings.h>
#include <seiscomp/messaging/status.h>


using namespace bs;


namespace Seiscomp {
namespace Applications {


// Definition of the parser grammar
struct MFilterParser : public bs::grammar<MFilterParser> {
	// Parser ids
	enum ParserID { rexpID = 1, notLexpID, groupID, expressionID };

	template <typename ScannerT>
	struct definition {
		definition(const MFilterParser& self) {
			TAG_TOKEN =
				bs::leaf_node_d[ (
					bs::str_p("time") |
					bs::str_p("hostname") |
					bs::str_p("clientname") |
					bs::str_p("programname") |
					bs::str_p("pid") |
					bs::str_p("cpuusage") |
					bs::str_p("totalmemory") |
					bs::str_p("clientmemoryusage") |
					bs::str_p("sentmessages") |
					bs::str_p("sentbytes") |
					bs::str_p("receivedmessages") |
					bs::str_p("receivedbytes") |
					bs::str_p("messagequeuesize") |
					bs::str_p("objectcount") |
					bs::str_p("address") |
					bs::str_p("uptime") |
					bs::str_p("responsetime")
					) ]
				;

			ROP_TOKEN =
				  (
				    bs::str_p("==")
				  | bs::str_p("!=")
				  | bs::str_p("<")
				  | bs::str_p(">")
				  | bs::str_p(">=")
				  | bs::str_p("<=")
				  )
				;

			rexp = TAG_TOKEN >> ROP_TOKEN >> bs::leaf_node_d[+(bs::alnum_p | bs::ch_p("_"))]
			;

			group =
				  bs::ch_p("(") >>
				  +expression >>
				  ch_p(")")
				;

			notLexp =
				bs::root_node_d[ bs::str_p("!") ] >> group
				;

			expression =
				(rexp | group | notLexp) >>
				*((bs::root_node_d[ str_p("&&") ] | bs::root_node_d[ str_p("||") ]) >> (rexp | group | notLexp))
				;

			BOOST_SPIRIT_DEBUG_NODE(TAG_TOKEN);
			BOOST_SPIRIT_DEBUG_NODE(ROP_TOKEN);
			BOOST_SPIRIT_DEBUG_NODE(rexp);
			BOOST_SPIRIT_DEBUG_NODE(group);
			BOOST_SPIRIT_DEBUG_NODE(expression);

		}


		const bs::rule<ScannerT, bs::parser_context<>, bs::parser_tag<expressionID> >&
		start() const {
			return expression;
		}

		bs::rule<ScannerT, bs::parser_context<>, bs::parser_tag<rexpID> >       rexp;
		bs::rule<ScannerT, bs::parser_context<>, bs::parser_tag<notLexpID> >    notLexp;
		bs::rule<ScannerT, bs::parser_context<>, bs::parser_tag<groupID> >      group;
		bs::rule<ScannerT, bs::parser_context<>, bs::parser_tag<expressionID> > expression;
		bs::rule<ScannerT> TAG_TOKEN, ROP_TOKEN, LOP_TOKEN;
	};

};



// Filter interface
template <typename Interface>
class MBinaryOperator : public Interface {
	public:
		MBinaryOperator() : _lhs(0), _rhs(0) {}

	public:
		void setOperants(void* lhs, void* rhs) {
			_lhs = lhs;
			_rhs = rhs;
		}

	protected:
		void* _lhs;
		void* _rhs;
};


template <typename Interface>
class MUnaryOperator : public Interface {
	public:
		MUnaryOperator() : _lhs(0) {}
	public:
		void setOperant(void* rhs) {
			_lhs = rhs;
		}
	protected:
		void* _lhs;
};


class MFilterInterface {
	public:
		virtual ~MFilterInterface() {}
		virtual bool eval(const ClientInfoData& clientData) const = 0;
};


#define DEFINE_LOPERATOR(name, op) \
class name : public MBinaryOperator<MFilterInterface> { \
	public: \
		virtual bool eval(const ClientInfoData& clientData) const { \
			if (!_lhs || !_rhs) \
				return false; \
			return ((MFilterInterface*)_lhs)->eval(clientData) op ((MFilterInterface*)_rhs)->eval(clientData); \
		} \
};

DEFINE_LOPERATOR(MAndOperator, &&)
DEFINE_LOPERATOR(MOrOperator, ||)

class MNotOperator : public MUnaryOperator<MFilterInterface> {
	public:
		virtual bool eval(const ClientInfoData& clientData) const {
			if (!_lhs)
				return false;
			return !((MFilterInterface*)_lhs)->eval(clientData);
		}
};


template <Client::Status::ETag tag>
bool getClientData(const ClientInfoData &clientData, typename Client::StatusT<tag>::Type &val) {
	ClientInfoData::const_iterator it = clientData.find(tag);
	if ( it == clientData.end() )
		return false;
	if ( !Core::fromString(val, it->second) )
		return false;
	return true;
}


#define DEFINE_ROPERATOR(name, op) \
class name : public MBinaryOperator<MFilterInterface> { \
	public: \
		virtual bool eval(const ClientInfoData& clientData) const { \
			switch( *((Client::Status::Tag*)_lhs) ) { \
				case Client::Status::Hostname: { \
					Client::StatusT<Client::Status::Hostname>::Type val; \
					if ( !getClientData<Client::Status::Hostname>(clientData, val) ) \
						return false; \
					return val op *((Client::StatusT<Client::Status::Hostname>::Type*)_rhs); \
				} \
				case Client::Status::Clientname: { \
					Client::StatusT<Client::Status::Clientname>::Type val; \
					if ( !getClientData<Client::Status::Clientname>(clientData, val) ) \
						return false; \
					return val op *((Client::StatusT<Client::Status::Clientname>::Type*)_rhs); \
				} \
				case Client::Status::Programname: { \
					Client::StatusT<Client::Status::Programname>::Type val; \
					if ( !getClientData<Client::Status::Programname>(clientData, val) ) \
						return false; \
					return val op *((Client::StatusT<Client::Status::Programname>::Type*)_rhs); \
				} \
				case Client::Status::PID: { \
					Client::StatusT<Client::Status::PID>::Type val; \
					if ( !getClientData<Client::Status::PID>(clientData, val) ) \
						return false; \
					return val op *((Client::StatusT<Client::Status::PID>::Type*)_rhs); \
				} \
				case Client::Status::CPUUsage: { \
					Client::StatusT<Client::Status::CPUUsage>::Type val; \
					if ( !getClientData<Client::Status::CPUUsage>(clientData, val) ) \
						return false; \
					return val op *((Client::StatusT<Client::Status::CPUUsage>::Type*)_rhs); \
				} \
				case Client::Status::TotalMemory: { \
					Client::StatusT<Client::Status::TotalMemory>::Type val; \
					if ( !getClientData<Client::Status::TotalMemory>(clientData, val) ) \
						return false; \
					return val op *((Client::StatusT<Client::Status::TotalMemory>::Type*)_rhs); \
				} \
				case Client::Status::ClientMemoryUsage: { \
					Client::StatusT<Client::Status::ClientMemoryUsage>::Type val; \
					if ( !getClientData<Client::Status::ClientMemoryUsage>(clientData, val) ) \
						return false; \
					return val op *((Client::StatusT<Client::Status::ClientMemoryUsage>::Type*)_rhs); \
				} \
				case Client::Status::SentMessages: { \
					Client::StatusT<Client::Status::SentMessages>::Type val; \
					if ( !getClientData<Client::Status::SentMessages>(clientData, val) ) \
						return false; \
					return val op *((Client::StatusT<Client::Status::SentMessages>::Type*)_rhs); \
				} \
				case Client::Status::SentBytes: { \
					Client::StatusT<Client::Status::SentBytes>::Type val; \
					if ( !getClientData<Client::Status::SentBytes>(clientData, val) ) \
						return false; \
					return val op *((Client::StatusT<Client::Status::SentBytes>::Type*)_rhs); \
				} \
				case Client::Status::ReceivedMessages: { \
					Client::StatusT<Client::Status::ReceivedMessages>::Type val; \
					if ( !getClientData<Client::Status::ReceivedMessages>(clientData, val) ) \
						return false; \
					return val op *((Client::StatusT<Client::Status::ReceivedMessages>::Type*)_rhs); \
				} \
				case Client::Status::ReceivedBytes: { \
					Client::StatusT<Client::Status::ReceivedBytes>::Type val; \
					if ( !getClientData<Client::Status::ReceivedBytes>(clientData, val) ) \
						return false; \
					return val op *((Client::StatusT<Client::Status::ReceivedBytes>::Type*)_rhs); \
				} \
				case Client::Status::MessageQueueSize: { \
					Client::StatusT<Client::Status::MessageQueueSize>::Type val; \
					if ( !getClientData<Client::Status::MessageQueueSize>(clientData, val) ) \
						return false; \
					return val op *((Client::StatusT<Client::Status::MessageQueueSize>::Type*)_rhs); \
				} \
				case Client::Status::ObjectCount: { \
					Client::StatusT<Client::Status::ObjectCount>::Type val; \
					if ( !getClientData<Client::Status::ObjectCount>(clientData, val) ) \
						return false; \
					return val op *((Client::StatusT<Client::Status::ObjectCount>::Type*)_rhs); \
				} \
				case Client::Status::Address: { \
					Client::StatusT<Client::Status::Address>::Type val; \
					if ( !getClientData<Client::Status::Address>(clientData, val) ) \
						return false; \
					return val op *((Client::StatusT<Client::Status::Address>::Type*)_rhs); \
				} \
				case Client::Status::Uptime: { \
					Client::StatusT<Client::Status::Uptime>::Type val; \
					if ( !getClientData<Client::Status::Uptime>(clientData, val) ) \
						return false; \
					return val op *((Client::StatusT<Client::Status::Uptime>::Type*)_rhs); \
				} \
				case Client::Status::ResponseTime: { \
					Client::StatusT<Client::Status::ResponseTime>::Type val; \
					if ( !getClientData<Client::Status::ResponseTime>(clientData, val) ) \
						return false; \
					return val op *((Client::StatusT<Client::Status::ResponseTime>::Type*)_rhs); \
				} \
				default: \
					break; \
			} \
			return false; \
		} \
};


DEFINE_ROPERATOR(MEqOperator, ==)
DEFINE_ROPERATOR(MNeOperator, !=)
DEFINE_ROPERATOR(MLtOperator, <)
DEFINE_ROPERATOR(MGtOperator, >)
DEFINE_ROPERATOR(MLeOperator, <=)
DEFINE_ROPERATOR(MGeOperator, >=)



class MOperatorFactory {
	public:
		MFilterInterface* createOperator(const std::string& op) {
			if (op == "&&")
				return new MAndOperator;
			else if (op =="||")
				return new MOrOperator;
			else if (op == "!")
				return new MNotOperator;
			else if (op == "==")
				return new MEqOperator;
			else if (op == "!=")
				return new MNeOperator;
			else if (op == "<=")
				return new MLeOperator;
			else if (op == ">=")
				return new MGeOperator;
			else if (op == "<")
				return new MLtOperator;
			else if (op == ">")
				return new MGtOperator;
			else
				return NULL;
		}
};


class TagFactory {
	public:
		static void* CrateObjectFromTag(Client::Status::Tag tag, const std::string &valStr) {
				switch ( tag ) {
					case Client::Status::Hostname: {
						typedef Client::StatusT<Client::Status::Hostname>::Type Type;
						Type* object = new Type;
						if ( Core::fromString(*object, valStr) )
							return object;
					}
					case Client::Status::Clientname:{
						typedef Client::StatusT<Client::Status::Clientname>::Type Type;
						Type* object = new Type;
						if ( Core::fromString(*object, valStr) )
							return object;
					}
					case Client::Status::Programname:{
						typedef Client::StatusT<Client::Status::Programname>::Type Type;
						Type* object = new Type;
						if ( Core::fromString(*object, valStr) )
							return object;
					}
					case Client::Status::PID: {
						typedef Client::StatusT<Client::Status::PID>::Type Type;
						Type* object = new Type;
						if ( Core::fromString(*object, valStr) )
							return object;
					}
					case Client::Status::CPUUsage: {
						typedef Client::StatusT<Client::Status::CPUUsage>::Type Type;
						Type* object = new Type;
						if ( Core::fromString(*object, valStr) )
							return object;
					}
					case Client::Status::TotalMemory: {
						typedef Client::StatusT<Client::Status::TotalMemory>::Type Type;
						Type* object = new Type;
						if ( Core::fromString(*object, valStr) )
							return object;
					}
					case Client::Status::ClientMemoryUsage: {
						typedef Client::StatusT<Client::Status::ClientMemoryUsage>::Type Type;
						Type* object = new Type;
						if ( Core::fromString(*object, valStr) )
							return object;
					}
					case Client::Status::SentMessages: {
						typedef Client::StatusT<Client::Status::SentMessages>::Type Type;
						Type* object = new Type;
						if ( Core::fromString(*object, valStr) )
							return object;
					}
					case Client::Status::SentBytes: {
						typedef Client::StatusT<Client::Status::SentBytes>::Type Type;
						Type* object = new Type;
						if ( Core::fromString(*object, valStr) )
							return object;
					}
					case Client::Status::ReceivedMessages: {
						typedef Client::StatusT<Client::Status::ReceivedMessages>::Type Type;
						Type* object = new Type;
						if ( Core::fromString(*object, valStr) )
							return object;
					}
					case Client::Status::ReceivedBytes: {
						typedef Client::StatusT<Client::Status::ReceivedBytes>::Type Type;
						Type* object = new Type;
						if ( Core::fromString(*object, valStr) )
							return object;
					}
					case Client::Status::MessageQueueSize: {
						typedef Client::StatusT<Client::Status::MessageQueueSize>::Type Type;
						Type* object = new Type;
						if ( Core::fromString(*object, valStr) )
							return object;
					}
					case Client::Status::ObjectCount: {
						typedef Client::StatusT<Client::Status::ObjectCount>::Type Type;
						Type* object = new Type;
						if ( Core::fromString(*object, valStr) )
							return object;
					}
					case Client::Status::Address: {
						typedef Client::StatusT<Client::Status::Address>::Type Type;
						Type* object = new Type;
						if ( Core::fromString(*object, valStr) )
							return object;
					}
					case Client::Status::Uptime: {
						typedef Client::StatusT<Client::Status::Uptime>::Type Type;
						Type* object = new Type;
						if ( Core::fromString(*object, valStr) )
							return object;
					}
					case Client::Status::ResponseTime: {
						typedef Client::StatusT<Client::Status::ResponseTime>::Type Type;
						Type* object = new Type;
						if ( Core::fromString(*object, valStr) )
							return object;
					}
					default:
						break;
				}
				return NULL;
		}
};




typedef bs::tree_match<const char*>::tree_iterator iter_t;
MFilterInterface* evalParseTree(iter_t iter, MOperatorFactory& factory) {
	if ( iter->value.id() == MFilterParser::rexpID ) {
		SEISCOMP_DEBUG("= rexp ( %lu children ) =", (unsigned long)iter->children.size());

		std::string tagStr((iter->children.begin())->value.begin(), (iter->children.begin())->value.end());
		std::string opStr((iter->children.begin()+1)->value.begin(), (iter->children.begin()+1)->value.end());
		std::string valStr((iter->children.begin()+2)->value.begin(), (iter->children.begin()+2)->value.end());

		SEISCOMP_DEBUG("%s %s %s", tagStr.c_str(), opStr.c_str(), valStr.c_str());
		Client::Status::Tag *tag = new Client::Status::Tag;
		if ( !tag->fromString(tagStr) ) {
			delete tag;
			return NULL;
		}

		MFilterInterface* opPtr = factory.createOperator(opStr);
		if ( !opPtr ) {
			SEISCOMP_ERROR("Could not create operator %s", opStr.c_str());
			return NULL;
		}

		void* object = TagFactory::CrateObjectFromTag(*tag, valStr);
		if ( !object ) {
			SEISCOMP_ERROR("Could not allocate memory for %s = %s", tagStr.c_str(), valStr.c_str());
			delete tag;
			return NULL;
		}
		static_cast<MBinaryOperator<MFilterInterface>* >(opPtr)->setOperants(tag, object);
		return opPtr;

	}
	else if ( iter->value.id() == MFilterParser::notLexpID ) {
		SEISCOMP_DEBUG("= not_lexp ( %lu children ) =", (unsigned long)iter->children.size());
		std::string opStr(iter->value.begin(), iter->value.end());
		SEISCOMP_DEBUG("operator: %s", opStr.c_str());

		MFilterInterface* tmp = evalParseTree(iter->children.begin(), factory);
		if ( !tmp )
			return NULL;

		MFilterInterface* opPtr = factory.createOperator(opStr);
		if ( !opPtr ) {
			SEISCOMP_DEBUG("Could not create operator %s", opStr.c_str());
			return NULL;
		}
		static_cast<MUnaryOperator<MFilterInterface>* >(opPtr)->setOperant(tmp);
		return opPtr;
	}
	else if ( iter->value.id() == MFilterParser::groupID ) {
		SEISCOMP_DEBUG("= group ( %lu children ) =", (unsigned long)iter->children.size());
		return evalParseTree(iter->children.begin()+1, factory);
	}
	else if ( iter->value.id() == MFilterParser::expressionID ) {
		SEISCOMP_DEBUG("= expression ( %lu children ) =", (unsigned long)iter->children.size());
		if ( iter->children.size() != 2 ) {
			SEISCOMP_ERROR("Expression has more or less than 2 children (%lu)",
					(unsigned long)iter->children.size());
			return NULL;
		}
		std::string opStr(iter->value.begin(), iter->value.end());
		SEISCOMP_DEBUG("operator: %s", opStr.c_str());

		MFilterInterface* tmp = evalParseTree(iter->children.begin(), factory);;
		MFilterInterface* tmp1 = evalParseTree(iter->children.begin()+1, factory);;
		if (!tmp || !tmp1)
			return NULL;

		MFilterInterface* opPtr = factory.createOperator(opStr);
		if ( !opPtr ) {
			SEISCOMP_DEBUG("Could not create operator %s", opStr.c_str());
			return NULL;
		}
		static_cast<MBinaryOperator<MFilterInterface>* >(opPtr)->setOperants(tmp, tmp1);
		return opPtr;
	}
	else
		SEISCOMP_DEBUG("Expression not handled");

	SEISCOMP_DEBUG("Returning null");
	return NULL;
}


} // namespace Applications
} // namespace Seiscomp

#endif
