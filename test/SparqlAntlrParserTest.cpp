// Copyright 2021 - 2025, University of Freiburg
// Chair of Algorithms and Data Structures
// Authors: Johannes Kalmbach <kalmbach@cs.uni-freiburg.de>
//          Julian Mundhahs <mundhahj@cs.uni-freiburg.de>
//          Hannah Bast <bast@cs.uni-freiburg.de>

#include <gtest/gtest.h>

#include <iostream>
#include <type_traits>
#include <typeindex>
#include <utility>

#include "./SparqlExpressionTestHelpers.h"
#include "./util/GTestHelpers.h"
#include "./util/RuntimeParametersTestHelpers.h"
#include "./util/TripleComponentTestHelpers.h"
#include "QueryPlannerTestHelpers.h"
#include "SparqlAntlrParserTestHelpers.h"
#include "engine/sparqlExpressions/BlankNodeExpression.h"
#include "engine/sparqlExpressions/CountStarExpression.h"
#include "engine/sparqlExpressions/ExistsExpression.h"
#include "engine/sparqlExpressions/GroupConcatExpression.h"
#include "engine/sparqlExpressions/LiteralExpression.h"
#include "engine/sparqlExpressions/NaryExpression.h"
#include "engine/sparqlExpressions/NowDatetimeExpression.h"
#include "engine/sparqlExpressions/RandomExpression.h"
#include "engine/sparqlExpressions/RegexExpression.h"
#include "engine/sparqlExpressions/RelationalExpressions.h"
#include "engine/sparqlExpressions/SampleExpression.h"
#include "engine/sparqlExpressions/StdevExpression.h"
#include "engine/sparqlExpressions/UuidExpressions.h"
#include "global/RuntimeParameters.h"
#include "parser/ConstructClause.h"
#include "parser/Iri.h"
#include "parser/SparqlParserHelpers.h"
#include "parser/sparqlParser/SparqlQleverVisitor.h"
#include "util/AllocatorTestHelpers.h"
#include "util/SourceLocation.h"

namespace {
using namespace sparqlParserHelpers;
namespace m = matchers;
using Parser = SparqlAutomaticParser;
using namespace std::literals;
using Var = Variable;
auto iri = ad_utility::testing::iri;

auto lit = ad_utility::testing::tripleComponentLiteral;

const ad_utility::HashMap<std::string, std::string> defaultPrefixMap{
    {std::string{QLEVER_INTERNAL_PREFIX_NAME},
     std::string{QLEVER_INTERNAL_PREFIX_IRI}}};

template <auto F, bool testInsideConstructTemplate = false>
auto parse =
    [](const string& input, SparqlQleverVisitor::PrefixMap prefixes = {},
       std::optional<ParsedQuery::DatasetClauses> clauses = std::nullopt,
       SparqlQleverVisitor::DisableSomeChecksOnlyForTesting disableSomeChecks =
           SparqlQleverVisitor::DisableSomeChecksOnlyForTesting::False) {
      ParserAndVisitor p{input, std::move(prefixes), std::move(clauses),
                         disableSomeChecks};
      if (testInsideConstructTemplate) {
        p.visitor_.setParseModeToInsideConstructTemplateForTesting();
      }
      return p.parseTypesafe(F);
    };

auto parseBlankNode = parse<&Parser::blankNode>;
auto parseBlankNodeConstruct = parse<&Parser::blankNode, true>;
auto parseCollection = parse<&Parser::collection>;
auto parseCollectionConstruct = parse<&Parser::collection, true>;
auto parseConstructTriples = parse<&Parser::constructTriples>;
auto parseGraphNode = parse<&Parser::graphNode>;
auto parseGraphNodeConstruct = parse<&Parser::graphNode, true>;
auto parseObjectList = parse<&Parser::objectList>;
auto parsePropertyList = parse<&Parser::propertyList>;
auto parsePropertyListNotEmpty = parse<&Parser::propertyListNotEmpty>;
auto parseSelectClause = parse<&Parser::selectClause>;
auto parseTriplesSameSubject = parse<&Parser::triplesSameSubject>;
auto parseTriplesSameSubjectConstruct =
    parse<&Parser::triplesSameSubject, true>;
auto parseVariable = parse<&Parser::var>;
auto parseVarOrTerm = parse<&Parser::varOrTerm>;
auto parseVerb = parse<&Parser::verb>;

template <auto Clause, bool parseInsideConstructTemplate = false,
          typename Value = decltype(parse<Clause>("").resultOfParse_)>
struct ExpectCompleteParse {
  SparqlQleverVisitor::PrefixMap prefixMap_ = {};
  SparqlQleverVisitor::DisableSomeChecksOnlyForTesting disableSomeChecks =
      SparqlQleverVisitor::DisableSomeChecksOnlyForTesting::False;

  auto operator()(const string& input, const Value& value,
                  ad_utility::source_location l =
                      ad_utility::source_location::current()) const {
    return operator()(input, value, prefixMap_, l);
  };

  auto operator()(const string& input,
                  const testing::Matcher<const Value&>& matcher,
                  ad_utility::source_location l =
                      ad_utility::source_location::current()) const {
    return operator()(input, matcher, prefixMap_, l);
  };

  auto operator()(const string& input, const Value& value,
                  SparqlQleverVisitor::PrefixMap prefixMap,
                  ad_utility::source_location l =
                      ad_utility::source_location::current()) const {
    return operator()(input, testing::Eq(value), std::move(prefixMap), l);
  };

  auto operator()(const string& input,
                  const testing::Matcher<const Value&>& matcher,
                  SparqlQleverVisitor::PrefixMap prefixMap,
                  ad_utility::source_location l =
                      ad_utility::source_location::current()) const {
    auto tr = generateLocationTrace(l, "successful parsing was expected here");
    EXPECT_NO_THROW({
      return expectCompleteParse(
          parse<Clause, parseInsideConstructTemplate>(
              input, std::move(prefixMap), std::nullopt, disableSomeChecks),
          matcher, l);
    });
  };

  auto operator()(const string& input,
                  const testing::Matcher<const Value&>& matcher,
                  ParsedQuery::DatasetClauses activeDatasetClauses,
                  ad_utility::source_location l =
                      ad_utility::source_location::current()) const {
    auto tr = generateLocationTrace(l, "successful parsing was expected here");
    EXPECT_NO_THROW({
      return expectCompleteParse(
          parse<Clause, parseInsideConstructTemplate>(
              input, {}, std::move(activeDatasetClauses), disableSomeChecks),
          matcher, l);
    });
  };
};

template <auto Clause>
struct ExpectParseFails {
  SparqlQleverVisitor::PrefixMap prefixMap_ = {};
  SparqlQleverVisitor::DisableSomeChecksOnlyForTesting disableSomeChecks =
      SparqlQleverVisitor::DisableSomeChecksOnlyForTesting::False;

  auto operator()(
      const string& input,
      const testing::Matcher<const std::string&>& messageMatcher = ::testing::_,
      ad_utility::source_location l = ad_utility::source_location::current()) {
    return operator()(input, prefixMap_, messageMatcher, l);
  }

  auto operator()(
      const string& input, SparqlQleverVisitor::PrefixMap prefixMap,
      const testing::Matcher<const std::string&>& messageMatcher = ::testing::_,
      ad_utility::source_location l = ad_utility::source_location::current()) {
    auto trace = generateLocationTrace(l);
    AD_EXPECT_THROW_WITH_MESSAGE(
        parse<Clause>(input, std::move(prefixMap), {}, disableSomeChecks),
        messageMatcher);
  }
};

// TODO: make function that creates both the complete and fails parser. and use
// them with structured binding.

auto nil = "<http://www.w3.org/1999/02/22-rdf-syntax-ns#nil>";
auto first = "<http://www.w3.org/1999/02/22-rdf-syntax-ns#first>";
auto rest = "<http://www.w3.org/1999/02/22-rdf-syntax-ns#rest>";
auto type = "<http://www.w3.org/1999/02/22-rdf-syntax-ns#type>";

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::StrEq;
}  // namespace

TEST(SparqlParser, NumericLiterals) {
  auto expectNumericLiteral = ExpectCompleteParse<&Parser::numericLiteral>{};
  auto expectNumericLiteralFails = ExpectParseFails<&Parser::numericLiteral>{};
  expectNumericLiteral("3.0", m::NumericLiteralDouble(3.0));
  expectNumericLiteral("3.0e2", m::NumericLiteralDouble(300.0));
  expectNumericLiteral("3.0e-2", m::NumericLiteralDouble(0.030));
  expectNumericLiteral("3", m::NumericLiteralInt(3ll));
  expectNumericLiteral("-3.0", m::NumericLiteralDouble(-3.0));
  expectNumericLiteral("-3", m::NumericLiteralInt(-3ll));
  expectNumericLiteral("+3", m::NumericLiteralInt(3ll));
  expectNumericLiteral("+3.02", m::NumericLiteralDouble(3.02));
  expectNumericLiteral("+3.1234e12", m::NumericLiteralDouble(3123400000000.0));
  expectNumericLiteral(".234", m::NumericLiteralDouble(0.234));
  expectNumericLiteral("+.0123", m::NumericLiteralDouble(0.0123));
  expectNumericLiteral("-.5123", m::NumericLiteralDouble(-0.5123));
  expectNumericLiteral(".234e4", m::NumericLiteralDouble(2340.0));
  expectNumericLiteral("+.0123E-3", m::NumericLiteralDouble(0.0000123));
  expectNumericLiteral("-.5123E12", m::NumericLiteralDouble(-512300000000.0));
  expectNumericLiteralFails("1000000000000000000000000000000000000");
  expectNumericLiteralFails("-99999999999999999999");
  expectNumericLiteralFails("12E400");
  expectNumericLiteralFails("-4.2E550");
}

TEST(SparqlParser, Prefix) {
  SparqlQleverVisitor::PrefixMap prefixMap{{"wd", "<www.wikidata.org/>"}};

  {
    ParserAndVisitor p{"PREFIX wd: <www.wikidata.org/>"};
    auto defaultPrefixes = p.visitor_.prefixMap();
    ASSERT_EQ(defaultPrefixes.size(), 0);
    p.visitor_.visit(p.parser_.prefixDecl());
    auto prefixes = p.visitor_.prefixMap();
    ASSERT_EQ(prefixes.size(), 1);
    ASSERT_EQ(prefixes.at("wd"), "<www.wikidata.org/>");
  }
  expectCompleteParse(parse<&Parser::pnameLn>("wd:bimbam", prefixMap),
                      StrEq("<www.wikidata.org/bimbam>"));
  expectCompleteParse(parse<&Parser::pnameNs>("wd:", prefixMap),
                      StrEq("<www.wikidata.org/>"));
  expectCompleteParse(parse<&Parser::prefixedName>("wd:bimbam", prefixMap),
                      StrEq("<www.wikidata.org/bimbam>"));
  expectIncompleteParse(
      parse<&Parser::iriref>("<somethingsomething> <rest>", prefixMap),
      "<rest>", testing::StrEq("<somethingsomething>"));
}

TEST(SparqlExpressionParser, First) {
  string s = "(5 * 5 ) bimbam";
  // This is an example on how to access a certain parsed substring.
  /*
  LOG(INFO) << context->getText() << std::endl;
  LOG(INFO) << p.parser_.getTokenStream()
                   ->getTokenSource()
                   ->getInputStream()
                   ->toString()
            << std::endl;
  LOG(INFO) << p.parser_.getCurrentToken()->getStartIndex() << std::endl;
   */
  auto resultofParse = parse<&Parser::expression>(s);
  EXPECT_EQ(resultofParse.remainingText_.length(), 6);
  auto resultAsExpression = std::move(resultofParse.resultOfParse_);

  VariableToColumnMap map;
  ad_utility::AllocatorWithLimit<Id> alloc{
      ad_utility::testing::makeAllocator()};
  IdTable table{alloc};
  LocalVocab localVocab;
  sparqlExpression::EvaluationContext input{
      *ad_utility::testing::getQec(),
      map,
      table,
      alloc,
      localVocab,
      std::make_shared<ad_utility::CancellationHandle<>>(),
      sparqlExpression::EvaluationContext::TimePoint::max()};
  auto result = resultAsExpression->evaluate(&input);
  AD_CONTRACT_CHECK(std::holds_alternative<Id>(result));
  ASSERT_EQ(std::get<Id>(result).getDatatype(), Datatype::Int);
  ASSERT_EQ(25, std::get<Id>(result).getInt());
}

TEST(SparqlParser, ComplexConstructTemplate) {
  string input =
      "{ [?a ( ?b (?c) )] ?d [?e [?f ?g]] . "
      "<http://wallscope.co.uk/resource/olympics/medal/#something> a "
      "<http://wallscope.co.uk/resource/olympics/medal/#somethingelse> }";

  auto Blank = [](const std::string& label) { return BlankNode(true, label); };
  expectCompleteParse(
      parse<&Parser::constructTemplate>(input),
      m::ConstructClause(
          {{Blank("0"), Var("?a"), Blank("3")},
           {Blank("2"), Iri(first), Blank("1")},
           {Blank("2"), Iri(rest), Iri(nil)},
           {Blank("1"), Iri(first), Var("?c")},
           {Blank("1"), Iri(rest), Iri(nil)},
           {Blank("3"), Iri(first), Var("?b")},
           {Blank("3"), Iri(rest), Blank("2")},
           {Blank("0"), Var("?d"), Blank("4")},
           {Blank("4"), Var("?e"), Blank("5")},
           {Blank("5"), Var("?f"), Var("?g")},
           {Iri("<http://wallscope.co.uk/resource/olympics/medal/"
                "#something>"),
            Iri(type),
            Iri("<http://wallscope.co.uk/resource/olympics/medal/"
                "#somethingelse>")}}));
}

TEST(SparqlParser, GraphTerm) {
  auto expectGraphTerm = ExpectCompleteParse<&Parser::graphTerm>{};
  expectGraphTerm("1337", m::Literal("1337"));
  expectGraphTerm("true", m::Literal("true"));
  expectGraphTerm("[]", m::InternalVariable("0"));
  auto expectGraphTermConstruct =
      ExpectCompleteParse<&Parser::graphTerm, true>{};
  expectGraphTermConstruct("[]", m::BlankNode(true, "0"));
  {
    const std::string iri = "<http://dummy-iri.com#fragment>";
    expectCompleteParse(parse<&Parser::graphTerm>(iri), m::Iri(iri));
  }
  expectGraphTerm("\"abc\"", m::Literal("\"abc\""));
  expectGraphTerm("()", m::Iri(nil));
}

TEST(SparqlParser, RdfCollectionSingleVar) {
  expectCompleteParse(
      parseCollectionConstruct("( ?a )"),
      Pair(m::BlankNode(true, "0"),
           ElementsAre(ElementsAre(m::BlankNode(true, "0"), m::Iri(first),
                                   m::VariableVariant("?a")),
                       ElementsAre(m::BlankNode(true, "0"), m::Iri(rest),
                                   m::Iri(nil)))));
  expectCompleteParse(
      parseCollection("( ?a )"),
      Pair(m::VariableVariant("?_QLever_internal_variable_0"),
           ElementsAre(
               ElementsAre(m::VariableVariant("?_QLever_internal_variable_0"),
                           m::Iri(first), m::VariableVariant("?a")),
               ElementsAre(m::VariableVariant("?_QLever_internal_variable_0"),
                           m::Iri(rest), m::Iri(nil)))));
}

TEST(SparqlParser, RdfCollectionTripleVar) {
  auto Var = m::VariableVariant;
  auto Blank = [](const std::string& label) {
    return m::BlankNode(true, label);
  };
  auto BlankVar = [](int number) {
    return m::VariableVariant(
        absl::StrCat("?_QLever_internal_variable_", number));
  };
  expectCompleteParse(
      parseCollectionConstruct("( ?a ?b ?c )"),
      Pair(m::BlankNode(true, "2"),
           ElementsAre(ElementsAre(Blank("0"), m::Iri(first), Var("?c")),
                       ElementsAre(Blank("0"), m::Iri(rest), m::Iri(nil)),
                       ElementsAre(Blank("1"), m::Iri(first), Var("?b")),
                       ElementsAre(Blank("1"), m::Iri(rest), Blank("0")),
                       ElementsAre(Blank("2"), m::Iri(first), Var("?a")),
                       ElementsAre(Blank("2"), m::Iri(rest), Blank("1")))));
  expectCompleteParse(
      parseCollection("( ?a ?b ?c )"),
      Pair(BlankVar(2),
           ElementsAre(ElementsAre(BlankVar(0), m::Iri(first), Var("?c")),
                       ElementsAre(BlankVar(0), m::Iri(rest), m::Iri(nil)),
                       ElementsAre(BlankVar(1), m::Iri(first), Var("?b")),
                       ElementsAre(BlankVar(1), m::Iri(rest), BlankVar(0)),
                       ElementsAre(BlankVar(2), m::Iri(first), Var("?a")),
                       ElementsAre(BlankVar(2), m::Iri(rest), BlankVar(1)))));
}

TEST(SparqlParser, BlankNodeAnonymous) {
  expectCompleteParse(parseBlankNodeConstruct("[ \t\r\n]"),
                      m::BlankNode(true, "0"));
  expectCompleteParse(parseBlankNode("[ \t\r\n]"), m::InternalVariable("0"));
}

TEST(SparqlParser, BlankNodeLabelled) {
  expectCompleteParse(parseBlankNodeConstruct("_:label123"),
                      m::BlankNode(false, "label123"));
  expectCompleteParse(parseBlankNode("_:label123"),
                      m::InternalVariable("label123"));
}

TEST(SparqlParser, ConstructTemplateEmpty) {
  expectCompleteParse(parse<&Parser::constructTemplate>("{}"),
                      testing::Eq(std::nullopt));
}

TEST(SparqlParser, ConstructTriplesSingletonWithTerminator) {
  expectCompleteParse(parseConstructTriples("?a ?b ?c ."),
                      ElementsAre(ElementsAre(m::VariableVariant("?a"),
                                              m::VariableVariant("?b"),
                                              m::VariableVariant("?c"))));
}

TEST(SparqlParser, ConstructTriplesWithTerminator) {
  auto IsVar = m::VariableVariant;
  expectCompleteParse(
      parseConstructTriples("?a ?b ?c . ?d ?e ?f . ?g ?h ?i ."),
      ElementsAre(
          ElementsAre(IsVar("?a"), IsVar("?b"), IsVar("?c")),
          ElementsAre(IsVar("?d"), IsVar("?e"), IsVar("?f")),
          ElementsAre(IsVar("?g"), IsVar("?h"), m::VariableVariant("?i"))));
}

TEST(SparqlParser, TriplesSameSubjectVarOrTerm) {
  expectCompleteParse(parseConstructTriples("?a ?b ?c"),
                      ElementsAre(ElementsAre(m::VariableVariant("?a"),
                                              m::VariableVariant("?b"),
                                              m::VariableVariant("?c"))));
}

TEST(SparqlParser, TriplesSameSubjectTriplesNodeWithPropertyList) {
  expectCompleteParse(
      parseTriplesSameSubjectConstruct("(?a) ?b ?c"),
      ElementsAre(
          ElementsAre(m::BlankNode(true, "0"), m::Iri(first),
                      m::VariableVariant("?a")),
          ElementsAre(m::BlankNode(true, "0"), m::Iri(rest), m::Iri(nil)),
          ElementsAre(m::BlankNode(true, "0"), m::VariableVariant("?b"),
                      m::VariableVariant("?c"))));
  expectCompleteParse(
      parseTriplesSameSubject("(?a) ?b ?c"),
      ElementsAre(
          ElementsAre(m::VariableVariant("?_QLever_internal_variable_0"),
                      m::Iri(first), m::VariableVariant("?a")),
          ElementsAre(m::VariableVariant("?_QLever_internal_variable_0"),
                      m::Iri(rest), m::Iri(nil)),
          ElementsAre(m::VariableVariant("?_QLever_internal_variable_0"),
                      m::VariableVariant("?b"), m::VariableVariant("?c"))));
}

TEST(SparqlParser, TriplesSameSubjectTriplesNodeEmptyPropertyList) {
  expectCompleteParse(
      parseTriplesSameSubjectConstruct("(?a)"),
      ElementsAre(
          ElementsAre(m::BlankNode(true, "0"), m::Iri(first),
                      m::VariableVariant("?a")),
          ElementsAre(m::BlankNode(true, "0"), m::Iri(rest), m::Iri(nil))));
  expectCompleteParse(
      parseTriplesSameSubject("(?a)"),
      ElementsAre(
          ElementsAre(m::VariableVariant("?_QLever_internal_variable_0"),
                      m::Iri(first), m::VariableVariant("?a")),
          ElementsAre(m::VariableVariant("?_QLever_internal_variable_0"),
                      m::Iri(rest), m::Iri(nil))));
}

TEST(SparqlParser, TriplesSameSubjectBlankNodePropertyList) {
  auto doTest = ad_utility::ApplyAsValueIdentity{[](auto allowPath) {
    auto input = "[ ?x ?y ] ?a ?b";
    auto [output, internal] = [&input, allowPath]() {
      if constexpr (allowPath) {
        return std::pair(parse<&Parser::triplesSameSubjectPath>(input),
                         m::InternalVariable("0"));
      } else {
        return std::pair(parse<&Parser::triplesSameSubject, true>(input),
                         m::BlankNode(true, "0"));
      }
    }();

    auto var = m::VariableVariant;
    expectCompleteParse(
        output, UnorderedElementsAre(
                    ::testing::FieldsAre(internal, var("?x"), var("?y")),
                    ::testing::FieldsAre(internal, var("?a"), var("?b"))));
  }};
  doTest.template operator()<true>();
  doTest.template operator()<false>();
}

TEST(SparqlParser, PropertyList) {
  expectCompleteParse(
      parsePropertyList("a ?a"),
      Pair(ElementsAre(ElementsAre(m::Iri(type), m::VariableVariant("?a"))),
           IsEmpty()));
}

TEST(SparqlParser, EmptyPropertyList) {
  expectCompleteParse(parsePropertyList(""), Pair(IsEmpty(), IsEmpty()));
}

TEST(SparqlParser, PropertyListNotEmptySingletonWithTerminator) {
  expectCompleteParse(
      parsePropertyListNotEmpty("a ?a ;"),
      Pair(ElementsAre(ElementsAre(m::Iri(type), m::VariableVariant("?a"))),
           IsEmpty()));
}

TEST(SparqlParser, PropertyListNotEmptyWithTerminator) {
  expectCompleteParse(
      parsePropertyListNotEmpty("a ?a ; a ?b ; a ?c ;"),
      Pair(ElementsAre(ElementsAre(m::Iri(type), m::VariableVariant("?a")),
                       ElementsAre(m::Iri(type), m::VariableVariant("?b")),
                       ElementsAre(m::Iri(type), m::VariableVariant("?c"))),
           IsEmpty()));
}

TEST(SparqlParser, VerbA) { expectCompleteParse(parseVerb("a"), m::Iri(type)); }

TEST(SparqlParser, VerbVariable) {
  expectCompleteParse(parseVerb("?a"), m::VariableVariant("?a"));
}

TEST(SparqlParser, ObjectListSingleton) {
  expectCompleteParse(parseObjectList("?a"),
                      Pair(ElementsAre(m::VariableVariant("?a")), IsEmpty()));
}

TEST(SparqlParser, ObjectList) {
  expectCompleteParse(
      parseObjectList("?a , ?b , ?c"),
      Pair(ElementsAre(m::VariableVariant("?a"), m::VariableVariant("?b"),
                       m::VariableVariant("?c")),
           IsEmpty()));
}

TEST(SparqlParser, BlankNodePropertyList) {
  auto doMatch = ad_utility::ApplyAsValueIdentity{[](auto InsideConstruct) {
    const auto blank = [InsideConstruct] {
      if constexpr (InsideConstruct) {
        return m::BlankNode(true, "0");
      } else {
        return m::InternalVariable("0");
      }
    }();
    expectCompleteParse(
        parse<&Parser::blankNodePropertyList, InsideConstruct>(
            "[ a ?a ; a ?b ; a ?c ]"),
        Pair(blank,
             ElementsAre(
                 ElementsAre(blank, m::Iri(type), m::VariableVariant("?a")),
                 ElementsAre(blank, m::Iri(type), m::VariableVariant("?b")),
                 ElementsAre(blank, m::Iri(type), m::VariableVariant("?c")))));
  }};
  doMatch.template operator()<true>();
  doMatch.template operator()<false>();
}

TEST(SparqlParser, GraphNodeVarOrTerm) {
  expectCompleteParse(parseGraphNode("?a"),
                      Pair(m::VariableVariant("?a"), IsEmpty()));
}

TEST(SparqlParser, GraphNodeTriplesNode) {
  expectCompleteParse(
      parseGraphNodeConstruct("(?a)"),
      Pair(m::BlankNode(true, "0"),
           ElementsAre(ElementsAre(m::BlankNode(true, "0"), m::Iri(first),
                                   m::VariableVariant("?a")),
                       ElementsAre(m::BlankNode(true, "0"), m::Iri(rest),
                                   m::Iri(nil)))));
  expectCompleteParse(
      parseGraphNode("(?a)"),
      Pair(m::VariableVariant("?_QLever_internal_variable_0"),
           ElementsAre(
               ElementsAre(m::VariableVariant("?_QLever_internal_variable_0"),
                           m::Iri(first), m::VariableVariant("?a")),
               ElementsAre(m::VariableVariant("?_QLever_internal_variable_0"),
                           m::Iri(rest), m::Iri(nil)))));
}

TEST(SparqlParser, VarOrTermVariable) {
  expectCompleteParse(parseVarOrTerm("?a"), m::VariableVariant("?a"));
}

TEST(SparqlParser, VarOrTermGraphTerm) {
  expectCompleteParse(parseVarOrTerm("()"), m::Iri(nil));
}

TEST(SparqlParser, Iri) {
  auto iri = &TripleComponent::Iri::fromIriref;
  auto expectIri = ExpectCompleteParse<&Parser::iri>{};
  expectIri("rdfs:label", iri("<http://www.w3.org/2000/01/rdf-schema#label>"),
            {{"rdfs", "<http://www.w3.org/2000/01/rdf-schema#>"}});
  expectIri(
      "rdfs:label", iri("<http://www.w3.org/2000/01/rdf-schema#label>"),
      {{"rdfs", "<http://www.w3.org/2000/01/rdf-schema#>"}, {"foo", "<bar#>"}});
  expectIri("<http://www.w3.org/2000/01/rdf-schema>"s,
            iri("<http://www.w3.org/2000/01/rdf-schema>"),
            SparqlQleverVisitor::PrefixMap{});
  expectIri("@en@rdfs:label"s,
            iri("@en@<http://www.w3.org/2000/01/rdf-schema#label>"),
            {{"rdfs", "<http://www.w3.org/2000/01/rdf-schema#>"}});
  expectIri("@en@<http://www.w3.org/2000/01/rdf-schema>"s,
            iri("@en@<http://www.w3.org/2000/01/rdf-schema>"),
            SparqlQleverVisitor::PrefixMap{});
}

TEST(SparqlParser, VarOrIriIri) {
  expectCompleteParse(parseVarOrTerm("<http://testiri>"),
                      m::Iri("<http://testiri>"));
}

TEST(SparqlParser, VariableWithQuestionMark) {
  expectCompleteParse(parseVariable("?variableName"),
                      m::Variable("?variableName"));
}

TEST(SparqlParser, VariableWithDollarSign) {
  expectCompleteParse(parseVariable("$variableName"),
                      m::Variable("?variableName"));
}

TEST(SparqlParser, Bind) {
  auto noChecks = SparqlQleverVisitor::DisableSomeChecksOnlyForTesting::True;
  auto expectBind = ExpectCompleteParse<&Parser::bind>{{}, noChecks};
  expectBind("BIND (10 - 5 as ?a)", m::Bind(Var{"?a"}, "10 - 5"));
  expectBind("bInD (?age - 10 As ?s)", m::Bind(Var{"?s"}, "?age - 10"));
}

TEST(SparqlParser, Integer) {
  auto expectInteger = ExpectCompleteParse<&Parser::integer>{};
  auto expectIntegerFails = ExpectParseFails<&Parser::integer>();
  expectInteger("1931", 1931ull);
  expectInteger("0", 0ull);
  expectInteger("18446744073709551615", 18446744073709551615ull);
  expectIntegerFails("18446744073709551616");
  expectIntegerFails("10000000000000000000000000000000000000000");
  expectIntegerFails("-1");
}

TEST(SparqlParser, LimitOffsetClause) {
  auto expectLimitOffset = ExpectCompleteParse<&Parser::limitOffsetClauses>{};
  auto expectLimitOffsetFails = ExpectParseFails<&Parser::limitOffsetClauses>();
  expectLimitOffset("LIMIT 10", m::LimitOffset(10, std::nullopt, 0));
  expectLimitOffset("OFFSET 31 LIMIT 12 TEXTLIMIT 14",
                    m::LimitOffset(12, 14, 31));
  expectLimitOffset("textlimit 999", m::LimitOffset(std::nullopt, 999, 0));
  expectLimitOffset("LIMIT      999", m::LimitOffset(999, std::nullopt, 0));
  expectLimitOffset("OFFSET 43",
                    m::LimitOffset(std::nullopt, std::nullopt, 43));
  expectLimitOffset("TEXTLIMIT 43 LIMIT 19", m::LimitOffset(19, 43, 0));
  expectLimitOffsetFails("LIMIT20");
  expectIncompleteParse(parse<&Parser::limitOffsetClauses>(
                            "Limit 10 TEXTLIMIT 20 offset 0 Limit 20"),
                        "Limit 20", m::LimitOffset(10ull, 20ull, 0ull));
}

TEST(SparqlParser, OrderCondition) {
  auto expectOrderCondition = ExpectCompleteParse<&Parser::orderCondition>{};
  auto expectOrderConditionFails = ExpectParseFails<&Parser::orderCondition>();
  // var
  expectOrderCondition("?test",
                       m::VariableOrderKeyVariant(Var{"?test"}, false));
  // brackettedExpression
  expectOrderCondition("DESC (?foo)",
                       m::VariableOrderKeyVariant(Var{"?foo"}, true));
  expectOrderCondition("ASC (?bar)",
                       m::VariableOrderKeyVariant(Var{"?bar"}, false));
  expectOrderCondition("ASC(?test - 5)",
                       m::ExpressionOrderKey("(?test - 5)", false));
  expectOrderCondition("DESC (10 || (5 && ?foo))",
                       m::ExpressionOrderKey("(10 || (5 && ?foo))", true));
  // constraint
  expectOrderCondition("(5 - ?mehr)",
                       m::ExpressionOrderKey("(5 - ?mehr)", false));
  expectOrderCondition("SUM(?i)", m::ExpressionOrderKey("SUM(?i)", false));
  expectOrderConditionFails("ASC SCORE(?i)");
}

TEST(SparqlParser, OrderClause) {
  auto expectOrderClause = ExpectCompleteParse<&Parser::orderClause>{};
  auto expectOrderClauseFails = ExpectParseFails<&Parser::orderClause>{};
  expectOrderClause(
      "ORDER BY ?test DESC(?foo - 5)",
      m::OrderKeys({VariableOrderKey{Var{"?test"}, false},
                    m::ExpressionOrderKeyTest{"(?foo - 5)", true}}));

  expectOrderClause("INTERNAL SORT BY ?test",
                    m::OrderKeys({VariableOrderKey{Var{"?test"}, false}},
                                 IsInternalSort::True));

  expectOrderClauseFails("INTERNAL SORT BY ?test DESC(?blubb)");
}

TEST(SparqlParser, GroupCondition) {
  auto expectGroupCondition = ExpectCompleteParse<&Parser::groupCondition>{};
  // variable
  expectGroupCondition("?test", m::VariableGroupKey("?test"));
  // expression without binding
  expectGroupCondition("(?test)", m::ExpressionGroupKey("?test"));
  // expression with binding
  expectGroupCondition("(?test AS ?mehr)",
                       m::AliasGroupKey("?test", Var{"?mehr"}));
  // builtInCall
  expectGroupCondition("COUNT(?test)", m::ExpressionGroupKey("COUNT(?test)"));
  // functionCall
  expectGroupCondition(
      "<http://www.opengis.net/def/function/geosparql/latitude>(?test)",
      m::ExpressionGroupKey(
          "<http://www.opengis.net/def/function/geosparql/latitude>(?test)"));
}

TEST(SparqlParser, GroupClause) {
  expectCompleteParse(
      parse<&Parser::groupClause>(
          "GROUP BY ?test (?foo - 10 as ?bar) COUNT(?baz)"),
      m::GroupKeys(
          {Var{"?test"}, std::pair{"?foo - 10", Var{"?bar"}}, "COUNT(?baz)"}));
}

TEST(SparqlParser, SolutionModifier) {
  auto expectSolutionModifier =
      ExpectCompleteParse<&Parser::solutionModifier>{};
  auto expectIncompleteParse = [](const string& input) {
    EXPECT_FALSE(
        parse<&Parser::solutionModifier>(input).remainingText_.empty());
  };
  using VOK = VariableOrderKey;

  expectSolutionModifier("", m::SolutionModifier({}, {}, {}, {}));
  // The following are no valid solution modifiers, because ORDER BY
  // has to appear before LIMIT.
  expectIncompleteParse("GROUP BY ?var LIMIT 10 ORDER BY ?var");
  expectSolutionModifier("TEXTLIMIT 1 LIMIT 10",
                         m::SolutionModifier({}, {}, {}, {10, 0, 1}));
  expectSolutionModifier(
      "GROUP BY ?var (?b - 10) HAVING (?var != 10) ORDER BY ?var TEXTLIMIT 1 "
      "LIMIT 10 OFFSET 2",
      m::SolutionModifier({Var{"?var"}, "?b - 10"}, {{"(?var != 10)"}},
                          {VOK{Var{"?var"}, false}}, {10, 2, 1}));
  expectSolutionModifier(
      "GROUP BY ?var HAVING (?foo < ?bar) ORDER BY (5 - ?var) TEXTLIMIT 21 "
      "LIMIT 2",
      m::SolutionModifier({Var{"?var"}}, {{"(?foo < ?bar)"}},
                          {std::pair{"(5 - ?var)", false}}, {2, 0, 21}));
  expectSolutionModifier(
      "GROUP BY (?var - ?bar) ORDER BY (5 - ?var)",
      m::SolutionModifier({"?var - ?bar"}, {}, {std::pair{"(5 - ?var)", false}},
                          {}));
}

TEST(SparqlParser, DataBlock) {
  auto expectDataBlock = ExpectCompleteParse<&Parser::dataBlock>{};
  auto expectDataBlockFails = ExpectParseFails<&Parser::dataBlock>();
  expectDataBlock("?test { \"foo\" }",
                  m::Values({Var{"?test"}}, {{lit("\"foo\"")}}));
  expectDataBlock("?test { 10.0 }", m::Values({Var{"?test"}}, {{10.0}}));
  expectDataBlock("?test { UNDEF }",
                  m::Values({Var{"?test"}}, {{TripleComponent::UNDEF{}}}));
  expectDataBlock("?test { false true }",
                  m::Values({Var{"?test"}}, {{false}, {true}}));
  expectDataBlock(
      R"(?foo { "baz" "bar" })",
      m::Values({Var{"?foo"}}, {{lit("\"baz\"")}, {lit("\"bar\"")}}));
  // TODO: Is this semantics correct?
  expectDataBlock(R"(( ) { ( ) })", m::Values({}, {{}}));
  expectDataBlock(R"(( ) { })", m::Values({}, {}));
  expectDataBlockFails("?test { ( ) }");
  expectDataBlock(R"(?foo { })", m::Values({Var{"?foo"}}, {}));
  expectDataBlock(R"(( ?foo ) { })", m::Values({Var{"?foo"}}, {}));
  expectDataBlockFails(R"(( ?foo ?bar ) { (<foo>) (<bar>) })");
  expectDataBlock(
      R"(( ?foo ?bar ) { (<foo> <bar>) })",
      m::Values({Var{"?foo"}, Var{"?bar"}}, {{iri("<foo>"), iri("<bar>")}}));
  expectDataBlock(
      R"(( ?foo ?bar ) { (<foo> "m") ("1" <bar>) })",
      m::Values({Var{"?foo"}, Var{"?bar"}},
                {{iri("<foo>"), lit("\"m\"")}, {lit("\"1\""), iri("<bar>")}}));
  expectDataBlock(
      R"(( ?foo ?bar ) { (<foo> "m") (<bar> <e>) (1 "f") })",
      m::Values({Var{"?foo"}, Var{"?bar"}}, {{iri("<foo>"), lit("\"m\"")},
                                             {iri("<bar>"), iri("<e>")},
                                             {1, lit("\"f\"")}}));
  // TODO<joka921/qup42> implement
  expectDataBlockFails(R"(( ) { (<foo>) })");
}

TEST(SparqlParser, InlineData) {
  auto expectInlineData = ExpectCompleteParse<&Parser::inlineData>{};
  auto expectInlineDataFails = ExpectParseFails<&Parser::inlineData>();
  expectInlineData("VALUES ?test { \"foo\" }",
                   m::InlineData({Var{"?test"}}, {{lit("\"foo\"")}}));
  // There must always be a block present for InlineData
  expectInlineDataFails("");
}

TEST(SparqlParser, propertyPaths) {
  auto expectPathOrVar = ExpectCompleteParse<&Parser::verbPathOrSimple>{};
  auto Iri = &PropertyPath::fromIri;
  auto Sequence = &PropertyPath::makeSequence;
  auto Alternative = &PropertyPath::makeAlternative;
  auto Inverse = &PropertyPath::makeInverse;
  auto Negated = &PropertyPath::makeNegated;
  auto ZeroOrMore = &PropertyPath::makeZeroOrMore;
  auto OneOrMore = &PropertyPath::makeOneOrMore;
  auto ZeroOrOne = &PropertyPath::makeZeroOrOne;
  using PrefixMap = SparqlQleverVisitor::PrefixMap;
  // Test all the base cases.
  // "a" is a special case. It is a valid PropertyPath.
  // It is short for "<http://www.w3.org/1999/02/22-rdf-syntax-ns#type>".
  expectPathOrVar("a",
                  Iri("<http://www.w3.org/1999/02/22-rdf-syntax-ns#type>"));
  expectPathOrVar(
      "@en@rdfs:label", Iri("@en@<http://www.w3.org/2000/01/rdf-schema#label>"),
      PrefixMap{{"rdfs", "<http://www.w3.org/2000/01/rdf-schema#>"}});
  EXPECT_THROW(parse<&Parser::verbPathOrSimple>("b"), ParseException);
  expectPathOrVar("test:foo", Iri("<http://www.example.com/foo>"),
                  {{"test", "<http://www.example.com/>"}});
  expectPathOrVar("?bar", Var{"?bar"});
  expectPathOrVar(":", Iri("<http://www.example.com/>"),
                  {{"", "<http://www.example.com/>"}});
  expectPathOrVar("<http://www.w3.org/1999/02/22-rdf-syntax-ns#type>",
                  Iri("<http://www.w3.org/1999/02/22-rdf-syntax-ns#type>"));
  // Test the basic combinators / | (...) + * ?.
  expectPathOrVar("a:a / a:b",
                  Sequence({Iri("<http://www.example.com/a>"),
                            Iri("<http://www.example.com/b>")}),
                  {{"a", "<http://www.example.com/>"}});
  expectPathOrVar("a:a | a:b",
                  Alternative({Iri("<http://www.example.com/a>"),
                               Iri("<http://www.example.com/b>")}),
                  {{"a", "<http://www.example.com/>"}});
  expectPathOrVar("^a:a", Inverse(Iri("<http://www.example.com/a>")),
                  {{"a", "<http://www.example.com/>"}});
  expectPathOrVar("!a:a", Negated({Iri("<http://www.example.com/a>")}),
                  {{"a", "<http://www.example.com/>"}});
  expectPathOrVar("!(a:a)", Negated({Iri("<http://www.example.com/a>")}),
                  {{"a", "<http://www.example.com/>"}});
  expectPathOrVar("!(a:a|^a:a)",
                  Negated({Iri("<http://www.example.com/a>"),
                           Inverse(Iri("<http://www.example.com/a>"))}),
                  {{"a", "<http://www.example.com/>"}});
  expectPathOrVar("!(a:a|^a:b|a:c|a:d|^a:e)",
                  Negated({Iri("<http://www.example.com/a>"),
                           Inverse(Iri("<http://www.example.com/b>")),
                           Iri("<http://www.example.com/c>"),
                           Iri("<http://www.example.com/d>"),
                           Inverse(Iri("<http://www.example.com/e>"))}),
                  {{"a", "<http://www.example.com/>"}});
  expectPathOrVar("(a:a)", Iri("<http://www.example.com/a>"),
                  {{"a", "<http://www.example.com/>"}});
  expectPathOrVar("a:a+", OneOrMore({Iri("<http://www.example.com/a>")}),
                  {{"a", "<http://www.example.com/>"}});
  {
    PropertyPath expected = ZeroOrOne({Iri("<http://www.example.com/a>")});
    expected.canBeNull_ = true;
    expectPathOrVar("a:a?", expected, {{"a", "<http://www.example.com/>"}});
  }
  {
    PropertyPath expected = ZeroOrMore({Iri("<http://www.example.com/a>")});
    expected.canBeNull_ = true;
    expectPathOrVar("a:a*", expected, {{"a", "<http://www.example.com/>"}});
  }
  // Test a bigger example that contains everything.
  {
    PropertyPath expected = Alternative(
        {Sequence({
             Iri("<http://www.example.com/a/a>"),
             ZeroOrMore({Iri("<http://www.example.com/b/b>")}),
         }),
         Iri("<http://www.example.com/c/c>"),
         OneOrMore(
             {Sequence({Iri("<http://www.example.com/a/a>"),
                        Iri("<http://www.example.com/b/b>"), Iri("<a/b/c>")})}),
         Negated({Iri("<http://www.w3.org/1999/02/22-rdf-syntax-ns#type>")}),
         Negated(
             {Inverse(Iri("<http://www.w3.org/1999/02/22-rdf-syntax-ns#type>")),
              Iri("<http://www.w3.org/1999/02/22-rdf-syntax-ns#type>"),
              Inverse(Iri("<http://www.example.com/a/a>"))})});
    expected.computeCanBeNull();
    expected.canBeNull_ = false;
    expectPathOrVar("a:a/b:b*|c:c|(a:a/b:b/<a/b/c>)+|!a|!(^a|a|^a:a)", expected,
                    {{"a", "<http://www.example.com/a/>"},
                     {"b", "<http://www.example.com/b/>"},
                     {"c", "<http://www.example.com/c/>"}});
  }
}

// _____________________________________________________________________________
TEST(SparqlParser, propertyPathsWriteToStream) {
  auto toString = [](const PropertyPath& path) {
    std::ostringstream os;
    path.writeToStream(os);
    return std::move(os).str();
  };
  {
    auto path = PropertyPath::makeNegated(
        {PropertyPath::makeInverse(PropertyPath::fromIri("<a>"))});
    EXPECT_EQ("!(^(<a>))", toString(path));
  }
  {
    auto path = PropertyPath::makeNegated(
        {PropertyPath::makeInverse(PropertyPath::fromIri("<a>")),
         PropertyPath::fromIri("<b>")});
    EXPECT_EQ("!(^(<a>)|<b>)", toString(path));
  }
  {
    auto path = PropertyPath::makeNegated({});
    EXPECT_EQ("!()", toString(path));
  }
}

TEST(SparqlParser, propertyListPathNotEmpty) {
  auto expectPropertyListPath =
      ExpectCompleteParse<&Parser::propertyListPathNotEmpty>{};
  auto Iri = &PropertyPath::fromIri;
  expectPropertyListPath("<bar> ?foo", {{{Iri("<bar>"), Var{"?foo"}}}, {}});
  expectPropertyListPath(
      "<bar> ?foo ; <mehr> ?f",
      {{{Iri("<bar>"), Var{"?foo"}}, {Iri("<mehr>"), Var{"?f"}}}, {}});
  expectPropertyListPath(
      "<bar> ?foo , ?baz",
      {{{Iri("<bar>"), Var{"?foo"}}, {Iri("<bar>"), Var{"?baz"}}}, {}});

  // A more complex example.
  auto V = m::VariableVariant;
  auto internal0 = m::InternalVariable("0");
  auto internal1 = m::InternalVariable("1");
  auto internal2 = m::InternalVariable("2");
  auto bar = m::Predicate("<bar>");
  expectPropertyListPath(
      "?x [?y ?z; <bar> ?b, ?p, [?d ?e], [<bar> ?e]]; ?u ?v",
      Pair(ElementsAre(Pair(V("?x"), internal0), Pair(V("?u"), V("?v"))),
           UnorderedElementsAre(
               ::testing::FieldsAre(internal0, V("?y"), V("?z")),
               ::testing::FieldsAre(internal0, bar, V("?b")),
               ::testing::FieldsAre(internal0, bar, V("?p")),
               ::testing::FieldsAre(internal0, bar, internal1),
               ::testing::FieldsAre(internal1, V("?d"), V("?e")),
               ::testing::FieldsAre(internal0, bar, internal2),
               ::testing::FieldsAre(internal2, bar, V("?e")))));
}

TEST(SparqlParser, triplesSameSubjectPath) {
  auto expectTriples = ExpectCompleteParse<&Parser::triplesSameSubjectPath>{};
  auto PathIri = &PropertyPath::fromIri;
  expectTriples("?foo <bar> ?baz",
                {{Var{"?foo"}, PathIri("<bar>"), Var{"?baz"}}});
  expectTriples("?foo <bar> ?baz ; <mehr> ?t",
                {{Var{"?foo"}, PathIri("<bar>"), Var{"?baz"}},
                 {Var{"?foo"}, PathIri("<mehr>"), Var{"?t"}}});
  expectTriples("?foo <bar> ?baz , ?t",
                {{Var{"?foo"}, PathIri("<bar>"), Var{"?baz"}},
                 {Var{"?foo"}, PathIri("<bar>"), Var{"?t"}}});
  expectTriples("?foo <bar> ?baz , ?t ; <mehr> ?d",
                {{Var{"?foo"}, PathIri("<bar>"), Var{"?baz"}},
                 {Var{"?foo"}, PathIri("<bar>"), Var{"?t"}},
                 {Var{"?foo"}, PathIri("<mehr>"), Var{"?d"}}});
  expectTriples("?foo <bar> ?baz ; <mehr> ?t , ?d",
                {{Var{"?foo"}, PathIri("<bar>"), Var{"?baz"}},
                 {Var{"?foo"}, PathIri("<mehr>"), Var{"?t"}},
                 {Var{"?foo"}, PathIri("<mehr>"), Var{"?d"}}});
  expectTriples("<foo> <bar> ?baz ; ?mehr \"a\"",
                {{Iri("<foo>"), PathIri("<bar>"), Var{"?baz"}},
                 {Iri("<foo>"), Var("?mehr"), Literal("\"a\"")}});
  auto expectTriplesConstruct =
      ExpectCompleteParse<&Parser::triplesSameSubjectPath, true>{};
  expectTriplesConstruct("_:1 <bar> ?baz", {{BlankNode(false, "1"),
                                             PathIri("<bar>"), Var{"?baz"}}});
  expectTriples(
      "_:one <bar> ?baz",
      {{Var{absl::StrCat(QLEVER_INTERNAL_BLANKNODE_VARIABLE_PREFIX, "one")},
        PathIri("<bar>"), Var{"?baz"}}});
  expectTriples("10.0 <bar> true",
                {{Literal(10.0), PathIri("<bar>"), Literal(true)}});
  expectTriples(
      "<foo> "
      "<http://qlever.cs.uni-freiburg.de/builtin-functions/contains-word> "
      "\"Berlin Freiburg\"",
      {{Iri("<foo>"),
        PathIri("<http://qlever.cs.uni-freiburg.de/builtin-functions/"
                "contains-word>"),
        Literal("\"Berlin Freiburg\"")}});
}

TEST(SparqlParser, SelectClause) {
  auto expectSelectClause = ExpectCompleteParse<&Parser::selectClause>{};
  auto expectSelectFails = ExpectParseFails<&Parser::selectClause>();

  using Alias = std::pair<string, ::Variable>;
  expectCompleteParse(parseSelectClause("SELECT *"),
                      m::AsteriskSelect(false, false));
  expectCompleteParse(parseSelectClause("SELECT DISTINCT *"),
                      m::AsteriskSelect(true, false));
  expectCompleteParse(parseSelectClause("SELECT REDUCED *"),
                      m::AsteriskSelect(false, true));
  expectSelectFails("SELECT DISTINCT REDUCED *");
  expectSelectFails("SELECT");
  expectSelectClause("SELECT ?foo", m::VariablesSelect({"?foo"}));
  expectSelectClause("SELECT ?foo ?baz ?bar",
                     m::VariablesSelect({"?foo", "?baz", "?bar"}));
  expectSelectClause("SELECT DISTINCT ?foo ?bar",
                     m::VariablesSelect({"?foo", "?bar"}, true, false));
  expectSelectClause("SELECT REDUCED ?foo ?bar ?baz",
                     m::VariablesSelect({"?foo", "?bar", "?baz"}, false, true));
  expectSelectClause("SELECT (10 as ?foo) ?bar",
                     m::Select({Alias{"10", Var{"?foo"}}, Var{"?bar"}}));
  expectSelectClause("SELECT DISTINCT (5 - 10 as ?m)",
                     m::Select({Alias{"5 - 10", Var{"?m"}}}, true, false));
  expectSelectClause(
      "SELECT (5 - 10 as ?m) ?foo (10 as ?bar)",
      m::Select({Alias{"5 - 10", "?m"}, Var{"?foo"}, Alias{"10", "?bar"}}));
}

TEST(SparqlParser, HavingCondition) {
  auto expectHavingCondition = ExpectCompleteParse<&Parser::havingCondition>{};
  auto expectHavingConditionFails =
      ExpectParseFails<&Parser::havingCondition>();

  expectHavingCondition("(?x <= 42.3)", m::stringMatchesFilter("(?x <= 42.3)"));
  expectHavingCondition("(?height > 1.7)",
                        m::stringMatchesFilter("(?height > 1.7)"));
  expectHavingCondition("(?predicate < \"<Z\")",
                        m::stringMatchesFilter("(?predicate < \"<Z\")"));
  expectHavingCondition("(LANG(?x) = \"en\")",
                        m::stringMatchesFilter("(LANG(?x) = \"en\")"));
}

TEST(SparqlParser, GroupGraphPattern) {
  auto expectGraphPattern =
      ExpectCompleteParse<&Parser::groupGraphPattern>{defaultPrefixMap};
  auto expectGroupGraphPatternFails =
      ExpectParseFails<&Parser::groupGraphPattern>{{}};
  auto DummyTriplesMatcher = m::Triples({{Var{"?x"}, Var{"?y"}, Var{"?z"}}});

  // Empty GraphPatterns.
  expectGraphPattern("{ }", m::GraphPattern());
  expectGraphPattern(
      "{ SELECT *  WHERE { } }",
      m::GraphPattern(m::SubSelect(::testing::_, m::GraphPattern())));

  SparqlTriple abc{Var{"?a"}, Var{"?b"}, Var{"?c"}};
  SparqlTriple def{Var{"?d"}, Var{"?e"}, Var{"?f"}};
  // Test the Components alone.
  expectGraphPattern("{ { ?a ?b ?c } }",
                     m::GraphPattern(m::GroupGraphPattern(m::Triples({abc}))));
  expectGraphPattern(
      "{ { ?a ?b ?c } UNION { ?d ?e ?f } }",
      m::GraphPattern(m::Union(m::GraphPattern(m::Triples({abc})),
                               m::GraphPattern(m::Triples({def})))));
  expectGraphPattern(
      "{ { ?a ?b ?c } UNION { ?d ?e ?f } UNION { ?g ?h ?i } }",
      m::GraphPattern(m::Union(
          m::GraphPattern(m::Union(m::GraphPattern(m::Triples({abc})),
                                   m::GraphPattern(m::Triples({def})))),
          m::GraphPattern(m::Triples({{Var{"?g"}, Var{"?h"}, Var{"?i"}}})))));
  expectGraphPattern("{ OPTIONAL { ?a <foo> <bar> } }",
                     m::GraphPattern(m::OptionalGraphPattern(
                         m::Triples({{Var{"?a"}, "<foo>", iri("<bar>")}}))));
  expectGraphPattern("{ MINUS { ?a <foo> <bar> } }",
                     m::GraphPattern(m::MinusGraphPattern(
                         m::Triples({{Var{"?a"}, "<foo>", iri("<bar>")}}))));
  expectGraphPattern(
      "{ FILTER (?a = 10) . ?x ?y ?z }",
      m::GraphPattern(false, {"(?a = 10)"}, DummyTriplesMatcher));
  expectGraphPattern("{ BIND (3 as ?c) }",
                     m::GraphPattern(m::Bind(Var{"?c"}, "3")));
  // The variables `?f` and `?b` have not been used before the BIND clause, but
  // this is valid according to the SPARQL standard.
  expectGraphPattern("{ BIND (?f - ?b as ?c) }",
                     m::GraphPattern(m::Bind(Var{"?c"}, "?f - ?b")));
  expectGraphPattern("{ VALUES (?a ?b) { (<foo> <bar>) (<a> <b>) } }",
                     m::GraphPattern(m::InlineData(
                         {Var{"?a"}, Var{"?b"}}, {{iri("<foo>"), iri("<bar>")},
                                                  {iri("<a>"), iri("<b>")}})));
  expectGraphPattern("{ ?x ?y ?z }", m::GraphPattern(DummyTriplesMatcher));
  expectGraphPattern(
      "{ SELECT *  WHERE { ?x ?y ?z } }",
      m::GraphPattern(m::SubSelect(m::AsteriskSelect(false, false),
                                   m::GraphPattern(DummyTriplesMatcher))));
  // Test mixes of the components to make sure that they interact correctly.
  expectGraphPattern(
      "{ ?x ?y ?z ; ?f <bar> }",
      m::GraphPattern(m::Triples({{Var{"?x"}, Var{"?y"}, Var{"?z"}},
                                  {Var{"?x"}, Var{"?f"}, iri("<bar>")}})));
  expectGraphPattern(
      "{ ?x ?y ?z . <foo> ?f <bar> }",
      m::GraphPattern(m::Triples({{Var{"?x"}, Var{"?y"}, Var{"?z"}},
                                  {iri("<foo>"), Var{"?f"}, iri("<bar>")}})));
  expectGraphPattern(
      "{ ?x <is-a> <Actor> . FILTER(?x != ?y) . ?y <is-a> <Actor> . "
      "FILTER(?y < ?x) }",
      m::GraphPattern(false, {"(?x != ?y)", "(?y < ?x)"},
                      m::Triples({{Var{"?x"}, "<is-a>", iri("<Actor>")},
                                  {Var{"?y"}, "<is-a>", iri("<Actor>")}})));
  expectGraphPattern(
      "{?x <is-a> \"Actor\" . FILTER(?x != ?y) . ?y <is-a> <Actor> . ?c "
      "ql:contains-entity ?x . ?c ql:contains-word \"coca* abuse\"}",
      m::GraphPattern(
          false, {"(?x != ?y)"},
          m::Triples(
              {{Var{"?x"}, "<is-a>", lit("\"Actor\"")},
               {Var{"?y"}, "<is-a>", iri("<Actor>")},
               {Var{"?c"}, std::string{CONTAINS_ENTITY_PREDICATE}, Var{"?x"}},
               {Var{"?c"}, std::string{CONTAINS_WORD_PREDICATE},
                lit("\"coca* abuse\"")}})));

  // Scoping of variables in combination with a BIND clause.
  expectGraphPattern(
      "{?x <is-a> <Actor> . BIND(10 - ?x as ?y) }",
      m::GraphPattern(m::Triples({{Var{"?x"}, "<is-a>", iri("<Actor>")}}),
                      m::Bind(Var{"?y"}, "10 - ?x")));
  expectGraphPattern(
      "{?x <is-a> <Actor> . BIND(10 - ?x as ?y) . ?a ?b ?c }",
      m::GraphPattern(m::Triples({{Var{"?x"}, "<is-a>", iri("<Actor>")}}),
                      m::Bind(Var{"?y"}, "10 - ?x"),
                      m::Triples({{Var{"?a"}, Var{"?b"}, Var{"?c"}}})));
  expectGroupGraphPatternFails("{?x <is-a> <Actor> . BIND(3 as ?x)}");
  expectGraphPattern(
      "{?x <is-a> <Actor> . {BIND(3 as ?x)} }",
      m::GraphPattern(m::Triples({{Var{"?x"}, "<is-a>", iri("<Actor>")}}),
                      m::GroupGraphPattern(m::Bind(Var{"?x"}, "3"))));
  expectGraphPattern(
      "{?x <is-a> <Actor> . OPTIONAL {BIND(3 as ?x)} }",
      m::GraphPattern(m::Triples({{Var{"?x"}, "<is-a>", iri("<Actor>")}}),
                      m::OptionalGraphPattern(m::Bind(Var{"?x"}, "3"))));
  expectGraphPattern(
      "{ {?x <is-a> <Actor>} UNION { BIND (3 as ?x)}}",
      m::GraphPattern(m::Union(
          m::GraphPattern(m::Triples({{Var{"?x"}, "<is-a>", iri("<Actor>")}})),
          m::GraphPattern(m::Bind(Var{"?x"}, "3")))));

  expectGraphPattern(
      "{?x <is-a> <Actor> . OPTIONAL { ?x <foo> <bar> } }",
      m::GraphPattern(m::Triples({{Var{"?x"}, "<is-a>", iri("<Actor>")}}),
                      m::OptionalGraphPattern(
                          m::Triples({{Var{"?x"}, "<foo>", iri("<bar>")}}))));
  expectGraphPattern(
      "{ SELECT *  WHERE { ?x ?y ?z } VALUES ?a { <a> <b> } }",
      m::GraphPattern(
          m::SubSelect(m::AsteriskSelect(false, false),
                       m::GraphPattern(DummyTriplesMatcher)),
          m::InlineData({Var{"?a"}}, {{iri("<a>")}, {iri("<b>")}})));
  expectGraphPattern("{ SERVICE <endpoint> { ?s ?p ?o } }",
                     m::GraphPattern(m::Service(
                         TripleComponent::Iri::fromIriref("<endpoint>"),
                         {Var{"?s"}, Var{"?p"}, Var{"?o"}}, "{ ?s ?p ?o }")));
  expectGraphPattern(
      "{ SERVICE <ep> { { SELECT ?s ?o WHERE { ?s ?p ?o } } } }",
      m::GraphPattern(m::Service(TripleComponent::Iri::fromIriref("<ep>"),
                                 {Var{"?s"}, Var{"?o"}},
                                 "{ { SELECT ?s ?o WHERE { ?s ?p ?o } } }")));

  expectGraphPattern(
      "{ SERVICE SILENT <ep> { { SELECT ?s ?o WHERE { ?s ?p ?o } } } }",
      m::GraphPattern(m::Service(
          TripleComponent::Iri::fromIriref("<ep>"), {Var{"?s"}, Var{"?o"}},
          "{ { SELECT ?s ?o WHERE { ?s ?p ?o } } }", "", true)));

  // SERVICE with a variable endpoint is not yet supported.
  expectGroupGraphPatternFails("{ SERVICE ?endpoint { ?s ?p ?o } }");

  expectGraphPattern("{ GRAPH ?g { ?x <is-a> <Actor> }}",
                     m::GraphPattern(m::GroupGraphPatternWithGraph(
                         Variable("?g"),
                         m::Triples({{Var{"?x"}, "<is-a>", iri("<Actor>")}}))));
  expectGraphPattern(
      "{ GRAPH <foo> { ?x <is-a> <Actor> }}",
      m::GraphPattern(m::GroupGraphPatternWithGraph(
          iri("<foo>"), m::Triples({{Var{"?x"}, "<is-a>", iri("<Actor>")}}))));
}

TEST(SparqlParser, RDFLiteral) {
  auto expectRDFLiteral = ExpectCompleteParse<&Parser::rdfLiteral>{
      {{"xsd", "<http://www.w3.org/2001/XMLSchema#>"}}};
  auto expectRDFLiteralFails = ExpectParseFails<&Parser::rdfLiteral>();

  expectRDFLiteral("   \"Astronaut\"^^xsd:string  \t",
                   "\"Astronaut\"^^<http://www.w3.org/2001/XMLSchema#string>"s);
  // The conversion to the internal date format
  // (":v:date:0000000000000001950-01-01T00:00:00") is done by
  // RdfStringParser<TokenizerCtre>::parseTripleObject(resultAsString) which
  // is only called at triplesBlock.
  expectRDFLiteral(
      "\"1950-01-01T00:00:00\"^^xsd:dateTime",
      "\"1950-01-01T00:00:00\"^^<http://www.w3.org/2001/XMLSchema#dateTime>"s);
  expectRDFLiteralFails(R"(?a ?b "The \"Moon\""@en .)");
}

TEST(SparqlParser, SelectQuery) {
  auto contains = [](const std::string& s) { return ::testing::HasSubstr(s); };
  auto expectSelectQuery =
      ExpectCompleteParse<&Parser::selectQuery>{defaultPrefixMap};
  auto expectSelectQueryFails = ExpectParseFails<&Parser::selectQuery>{};
  auto DummyGraphPatternMatcher =
      m::GraphPattern(m::Triples({{Var{"?x"}, Var{"?y"}, Var{"?z"}}}));
  using Graphs = ScanSpecificationAsTripleComponent::Graphs;

  // A matcher that matches the query `SELECT * { ?a <bar> ?foo}`, where the
  // FROM and FROM NAMED clauses can still be specified via arguments.
  auto selectABarFooMatcher = [](Graphs defaultGraphs = std::nullopt,
                                 Graphs namedGraphs = std::nullopt) {
    return m::SelectQuery(
        m::AsteriskSelect(),
        m::GraphPattern(m::Triples({{Var{"?a"}, "<bar>", Var{"?foo"}}})),
        defaultGraphs, namedGraphs);
  };
  expectSelectQuery("SELECT * WHERE { ?a <bar> ?foo }", selectABarFooMatcher());

  expectSelectQuery(
      "SELECT * FROM <x> FROM NAMED <y> WHERE { ?a <bar> ?foo }",
      selectABarFooMatcher(m::Graphs{TripleComponent::Iri::fromIriref("<x>")},
                           m::Graphs{TripleComponent::Iri::fromIriref("<y>")}));

  expectSelectQuery(
      "SELECT * WHERE { ?x ?y ?z }",
      m::SelectQuery(m::AsteriskSelect(), DummyGraphPatternMatcher));
  expectSelectQuery(
      "SELECT ?x WHERE { ?x ?y ?z . FILTER(?x != <foo>) } LIMIT 10 TEXTLIMIT 5",
      testing::AllOf(
          m::SelectQuery(
              m::Select({Var{"?x"}}),
              m::GraphPattern(false, {"(?x != <foo>)"},
                              m::Triples({{Var{"?x"}, Var{"?y"}, Var{"?z"}}}))),
          m::pq::LimitOffset({10, 0, 5})));

  // ORDER BY
  expectSelectQuery("SELECT ?x WHERE { ?x ?y ?z } ORDER BY ?y ",
                    testing::AllOf(m::SelectQuery(m::Select({Var{"?x"}}),
                                                  DummyGraphPatternMatcher),
                                   m::pq::OrderKeys({{Var{"?y"}, false}})));

  // Explicit GROUP BY
  expectSelectQuery("SELECT ?x WHERE { ?x ?y ?z } GROUP BY ?x",
                    testing::AllOf(m::SelectQuery(m::VariablesSelect({"?x"}),
                                                  DummyGraphPatternMatcher),
                                   m::pq::GroupKeys({Var{"?x"}})));
  expectSelectQuery(
      "SELECT (COUNT(?y) as ?a) WHERE { ?x ?y ?z } GROUP BY ?x",
      testing::AllOf(
          m::SelectQuery(m::Select({std::pair{"COUNT(?y)", Var{"?a"}}}),
                         DummyGraphPatternMatcher),
          m::pq::GroupKeys({Var{"?x"}})));

  expectSelectQuery(
      "SELECT (SUM(?x) as ?a) (COUNT(?y) + ?z AS ?b)  WHERE { ?x ?y ?z } GROUP "
      "BY ?z",
      testing::AllOf(
          m::SelectQuery(m::Select({std::pair{"SUM(?x)", Var{"?a"}},
                                    std::pair{"COUNT(?y) + ?z", Var{"?b"}}}),
                         DummyGraphPatternMatcher)));

  expectSelectQuery(
      "SELECT (SUM(?x) as ?a)  WHERE { ?x ?y ?z } GROUP "
      "BY ?z ORDER BY (COUNT(?y) + ?z)",
      testing::AllOf(
          m::SelectQuery(
              m::Select({std::pair{"SUM(?x)", Var{"?a"}}}, false, false,
                        {std::pair{"(COUNT(?y) + ?z)",
                                   Var{"?_QLever_internal_variable_0"}}}),
              DummyGraphPatternMatcher),
          m::pq::OrderKeys({{Var{"?_QLever_internal_variable_0"}, false}})));

  // It is also illegal to reuse a variable from the body of a query with a
  // GROUP BY as the target of an alias, even if it is the aggregated variable
  // itself.
  expectSelectQueryFails(
      "SELECT (SUM(?y) AS ?y) WHERE { ?x <is-a> ?y } GROUP BY ?x");

  // `SELECT *` is not allowed while grouping.
  expectSelectQueryFails("SELECT * WHERE { ?x ?y ?z } GROUP BY ?x");
  // When grouping selected variables must either be grouped by or aggregated.
  // `?y` is neither.
  expectSelectQueryFails("SELECT (?y as ?a) WHERE { ?x ?y ?z } GROUP BY ?x");

  // Explicit GROUP BY but the target of an alias is used twice.
  expectSelectQueryFails(
      "SELECT (?x AS ?z) (?x AS ?z) WHERE { ?x <p> ?y} GROUP BY ?x");

  // Explicit GROUP BY but the second alias uses the target of the first alias
  // as input.
  expectSelectQuery(
      "SELECT (?x AS ?a) (?a AS ?aa) WHERE { ?x ?y ?z} GROUP BY ?x",
      testing::AllOf(m::SelectQuery(m::Select({std::pair{"?x", Var{"?a"}},
                                               std::pair{"?a", Var{"?aa"}}}),
                                    DummyGraphPatternMatcher),
                     m::pq::GroupKeys({Var{"?x"}})));

  // Implicit GROUP BY.
  expectSelectQuery(
      "SELECT (SUM(?x) as ?a) (COUNT(?y) + AVG(?z) AS ?b)  WHERE { ?x ?y ?z }",
      testing::AllOf(m::SelectQuery(m::Select({std::pair{"SUM(?x)", Var{"?a"}},
                                               std::pair{"COUNT(?y) + AVG(?z)",
                                                         Var{"?b"}}}),
                                    DummyGraphPatternMatcher),
                     m::pq::GroupKeys({})));
  // Implicit GROUP BY but the variable `?x` is not aggregated.
  expectSelectQueryFails("SELECT ?x (SUM(?y) AS ?z) WHERE { ?x <p> ?y}");
  // Implicit GROUP BY but the variable `?x` is not aggregated inside the
  // expression that also contains the aggregate.
  expectSelectQueryFails("SELECT (?x + SUM(?y) AS ?z) WHERE { ?x <p> ?y}");

  // When there is no GROUP BY (implicit or explicit), the aliases are
  // equivalently transformed into BINDs and then deleted from the SELECT
  // clause.
  expectSelectQuery("SELECT (?x AS ?y) (?y AS ?z) WHERE { BIND(1 AS ?x)}",
                    m::SelectQuery(m::Select({Var("?y"), Var("?z")}),
                                   m::GraphPattern(m::Bind(Var("?x"), "1"),
                                                   m::Bind(Var("?y"), "?x"),
                                                   m::Bind(Var{"?z"}, "?y"))));

  // No GROUP BY but the target of an alias is used twice.
  expectSelectQueryFails("SELECT (?x AS ?z) (?x AS ?z) WHERE { ?x <p> ?y}",
                         contains("The target ?z of an AS clause was already "
                                  "used before in the SELECT clause."));

  // `?x` is selected twice. Once as variable and once as the result of an
  // alias. This is not allowed.
  expectSelectQueryFails(
      "SELECT ?x (?y as ?x) WHERE { ?x ?y ?z }",
      contains(
          "The target ?x of an AS clause was already used in the query body."));

  // HAVING is not allowed without GROUP BY
  expectSelectQueryFails(
      "SELECT ?x WHERE { ?x ?y ?z } HAVING (?x < 3)",
      contains("HAVING clause is only supported in queries with GROUP BY"));

  // The target of the alias (`?y`) is already bound in the WHERE clause. This
  // is forbidden by the SPARQL standard.
  expectSelectQueryFails(
      "SELECT (?x AS ?y) WHERE { ?x <is-a> ?y }",
      contains(
          "The target ?y of an AS clause was already used in the query body."));
}

TEST(SparqlParser, ConstructQuery) {
  auto expectConstructQuery =
      ExpectCompleteParse<&Parser::constructQuery>{defaultPrefixMap};
  auto expectConstructQueryFails = ExpectParseFails<&Parser::constructQuery>{};
  expectConstructQuery(
      "CONSTRUCT { } WHERE { ?a ?b ?c }",
      m::ConstructQuery({}, m::GraphPattern(m::Triples(
                                {{Var{"?a"}, Var{"?b"}, Var{"?c"}}}))));
  expectConstructQuery(
      "CONSTRUCT { ?a <foo> ?c . } WHERE { ?a ?b ?c }",
      testing::AllOf(m::ConstructQuery(
          {{Var{"?a"}, Iri{"<foo>"}, Var{"?c"}}},
          m::GraphPattern(m::Triples({{Var{"?a"}, Var{"?b"}, Var{"?c"}}})))));
  expectConstructQuery(
      "CONSTRUCT { ?a <foo> ?c . <bar> ?b <baz> } WHERE { ?a ?b ?c . FILTER(?a "
      "> 0) .}",
      m::ConstructQuery(
          {{Var{"?a"}, Iri{"<foo>"}, Var{"?c"}},
           {Iri{"<bar>"}, Var{"?b"}, Iri{"<baz>"}}},
          m::GraphPattern(false, {"(?a > 0)"},
                          m::Triples({{Var{"?a"}, Var{"?b"}, Var{"?c"}}}))));
  expectConstructQuery(
      "CONSTRUCT { ?a <foo> ?c . } WHERE { ?a ?b ?c } ORDER BY ?a LIMIT 10",
      testing::AllOf(
          m::ConstructQuery(
              {{Var{"?a"}, Iri{"<foo>"}, Var{"?c"}}},
              m::GraphPattern(m::Triples({{Var{"?a"}, Var{"?b"}, Var{"?c"}}}))),
          m::pq::LimitOffset({10}), m::pq::OrderKeys({{Var{"?a"}, false}})));
  // This case of the grammar is not useful without Datasets, but we still
  // support it.
  expectConstructQuery(
      "CONSTRUCT WHERE { ?a <foo> ?b }",
      m::ConstructQuery(
          {{Var{"?a"}, Iri{"<foo>"}, Var{"?b"}}},
          m::GraphPattern(m::Triples({{Var{"?a"}, "<foo>", Var{"?b"}}}))));

  // Blank nodes turn into variables inside WHERE.
  expectConstructQuery(
      "CONSTRUCT WHERE { [] <foo> ?b }",
      m::ConstructQuery(
          {{BlankNode{true, "0"}, Iri{"<foo>"}, Var{"?b"}}},
          m::GraphPattern(m::Triples(
              {{Var{absl::StrCat(QLEVER_INTERNAL_BLANKNODE_VARIABLE_PREFIX,
                                 "g_0")},
                "<foo>", Var{"?b"}}}))));

  // Test another variant to cover all cases.
  expectConstructQuery(
      "CONSTRUCT WHERE { <bar> ?foo \"Abc\"@en }",
      m::ConstructQuery(
          {{Iri{"<bar>"}, Var{"?foo"}, Literal{"\"Abc\"@en"}}},
          m::GraphPattern(m::Triples(
              {{iri("<bar>"), Var{"?foo"}, lit("\"Abc\"", "@en")}}))));
  // CONSTRUCT with datasets.
  expectConstructQuery(
      "CONSTRUCT { } FROM <foo> FROM NAMED <foo2> FROM NAMED <foo3> WHERE { }",
      m::ConstructQuery({}, m::GraphPattern(), m::Graphs{iri("<foo>")},
                        m::Graphs{iri("<foo2>"), iri("<foo3>")}));
}

// _____________________________________________________________________________
TEST(SparqlParser, ensureExceptionOnInvalidGraphTerm) {
  EXPECT_THROW(SparqlQleverVisitor::toGraphPattern(
                   {{Var{"?a"}, BlankNode{true, "0"}, Var{"?b"}}}),
               ad_utility::Exception);
  EXPECT_THROW(SparqlQleverVisitor::toGraphPattern(
                   {{Var{"?a"}, Literal{"\"Abc\""}, Var{"?b"}}}),
               ad_utility::Exception);
}

// Test that ASK queries are parsed as they should.
TEST(SparqlParser, AskQuery) {
  // Some helper functions and abbreviations.
  auto contains = [](const std::string& s) { return ::testing::HasSubstr(s); };
  auto expectAskQuery =
      ExpectCompleteParse<&Parser::askQuery>{defaultPrefixMap};
  auto expectAskQueryFails = ExpectParseFails<&Parser::askQuery>{};
  auto DummyGraphPatternMatcher =
      m::GraphPattern(m::Triples({{Var{"?x"}, Var{"?y"}, Var{"?z"}}}));
  using Graphs = ScanSpecificationAsTripleComponent::Graphs;

  // A matcher that matches the query `ASK { ?a <bar> ?foo}`, where the
  // FROM parts of the query can be specified via `defaultGraphs` and
  // the FROM NAMED parts can be specified via `namedGraphs`.
  auto selectABarFooMatcher = [](Graphs defaultGraphs = std::nullopt,
                                 Graphs namedGraphs = std::nullopt) {
    return m::AskQuery(
        m::GraphPattern(m::Triples({{Var{"?a"}, "<bar>", Var{"?foo"}}})),
        defaultGraphs, namedGraphs);
  };
  expectAskQuery("ASK { ?a <bar> ?foo }", selectABarFooMatcher());

  // ASK query with both a FROM and a FROM NAMED clause.
  Graphs defaultGraphs;
  defaultGraphs.emplace();
  defaultGraphs->insert(TripleComponent::Iri::fromIriref("<x>"));
  Graphs namedGraphs;
  namedGraphs.emplace();
  namedGraphs->insert(TripleComponent::Iri::fromIriref("<y>"));
  expectAskQuery("ASK FROM <x> FROM NAMED <y> WHERE { ?a <bar> ?foo }",
                 selectABarFooMatcher(defaultGraphs, namedGraphs));

  // ASK whether there are any triples at all.
  expectAskQuery("ASK { ?x ?y ?z }", m::AskQuery(DummyGraphPatternMatcher));

  // ASK queries may contain neither of LIMIT, OFFSET, or TEXTLIMIT.
  expectAskQueryFails("ASK WHERE { ?x ?y ?z . FILTER(?x != <foo>) } LIMIT 10");
  expectAskQueryFails("ASK WHERE { ?x ?y ?z . FILTER(?x != <foo>) } OFFSET 20");
  expectAskQueryFails(
      "ASK WHERE { ?x ?y ?z . FILTER(?x != <foo>) } TEXTLIMIT 30");

  // ASK with ORDER BY is allowed (even though the ORDER BY does not change the
  // result).
  expectAskQuery("ASK { ?x ?y ?z } ORDER BY ?y ",
                 testing::AllOf(m::AskQuery(DummyGraphPatternMatcher),
                                m::pq::OrderKeys({{Var{"?y"}, false}})));

  // ASK with GROUP BY is allowed.
  expectAskQuery("ASK { ?x ?y ?z } GROUP BY ?x",
                 testing::AllOf(m::AskQuery(DummyGraphPatternMatcher),
                                m::pq::GroupKeys({Var{"?x"}})));
  expectAskQuery("ASK { ?x ?y ?z } GROUP BY ?x",
                 testing::AllOf(m::AskQuery(DummyGraphPatternMatcher),
                                m::pq::GroupKeys({Var{"?x"}})));

  // HAVING is not allowed without GROUP BY
  expectAskQueryFails(
      "ASK { ?x ?y ?z } HAVING (?x < 3)",
      contains("HAVING clause is only supported in queries with GROUP BY"));
}

// Tests for additional features of the SPARQL parser.
TEST(SparqlParser, Query) {
  auto expectQuery = ExpectCompleteParse<&Parser::query>{defaultPrefixMap};
  auto expectQueryFails = ExpectParseFails<&Parser::query>{};
  auto contains = [](const std::string& s) { return ::testing::HasSubstr(s); };

  // Test that `_originalString` is correctly set.
  expectQuery(
      "SELECT * WHERE { ?a <bar> ?foo }",
      testing::AllOf(m::SelectQuery(m::AsteriskSelect(),
                                    m::GraphPattern(m::Triples(
                                        {{Var{"?a"}, "<bar>", Var{"?foo"}}}))),
                     m::pq::OriginalString("SELECT * WHERE { ?a <bar> ?foo }"),
                     m::VisibleVariables({Var{"?a"}, Var{"?foo"}})));
  expectQuery("SELECT * WHERE { ?x ?y ?z }",
              m::pq::OriginalString("SELECT * WHERE { ?x ?y ?z }"));
  expectQuery(
      "SELECT ?x WHERE { ?x ?y ?z } GROUP BY ?x",
      m::pq::OriginalString("SELECT ?x WHERE { ?x ?y ?z } GROUP BY ?x"));
  expectQuery(
      "PREFIX a: <foo> SELECT (COUNT(?y) as ?a) WHERE { ?x ?y ?z } GROUP BY ?x",
      m::pq::OriginalString("PREFIX a: <foo> SELECT (COUNT(?y) as ?a) WHERE { "
                            "?x ?y ?z } GROUP BY ?x"));

  // Test that visible variables are correctly set.
  expectQuery(
      "CONSTRUCT { ?a <foo> ?c . } WHERE { ?a ?b ?c }",
      testing::AllOf(
          m::ConstructQuery(
              {{Var{"?a"}, Iri{"<foo>"}, Var{"?c"}}},
              m::GraphPattern(m::Triples({{Var{"?a"}, Var{"?b"}, Var{"?c"}}}))),
          m::VisibleVariables({Var{"?a"}, Var{"?b"}, Var{"?c"}})));
  expectQuery(
      "CONSTRUCT { ?x <foo> <bar> } WHERE { ?x ?y ?z } LIMIT 10",
      testing::AllOf(
          m::ConstructQuery(
              {{Var{"?x"}, Iri{"<foo>"}, Iri{"<bar>"}}},
              m::GraphPattern(m::Triples({{Var{"?x"}, Var{"?y"}, Var{"?z"}}}))),
          m::pq::OriginalString(
              "CONSTRUCT { ?x <foo> <bar> } WHERE { ?x ?y ?z } LIMIT 10"),
          m::pq::LimitOffset({10}),
          m::VisibleVariables({Var{"?x"}, Var{"?y"}, Var{"?z"}})));

  // Construct query with GROUP BY
  expectQuery(
      "CONSTRUCT { ?x <foo> <bar> } WHERE { ?x ?y ?z } GROUP BY ?x",
      testing::AllOf(
          m::ConstructQuery(
              {{Var{"?x"}, Iri{"<foo>"}, Iri{"<bar>"}}},
              m::GraphPattern(m::Triples({{Var{"?x"}, Var{"?y"}, Var{"?z"}}}))),
          m::pq::OriginalString(
              "CONSTRUCT { ?x <foo> <bar> } WHERE { ?x ?y ?z } GROUP BY ?x"),
          m::VisibleVariables({Var{"?x"}, Var{"?y"}, Var{"?z"}})));

  // Construct query with GROUP BY, but a variable that is not grouped is used.
  expectQueryFails(
      "CONSTRUCT { ?x <foo> <bar> } WHERE { ?x ?y ?z } GROUP BY ?y");

  // The same two tests with `ASK` queries
  expectQuery(
      "ASK WHERE { ?x ?y ?z } GROUP BY ?x",
      testing::AllOf(
          m::AskQuery(

              m::GraphPattern(m::Triples({{Var{"?x"}, Var{"?y"}, Var{"?z"}}}))),
          m::pq::OriginalString("ASK WHERE { ?x ?y ?z } GROUP BY ?x"),
          m::VisibleVariables({Var{"?x"}, Var{"?y"}, Var{"?z"}})));

  // Test that the prologue is parsed properly. We use `m::Service` here
  // because the parsing of a SERVICE clause is the only place where the
  // prologue is explicitly passed on to a `parsedQuery::` object.
  expectQuery(
      "PREFIX doof: <http://doof.org/> "
      "SELECT * WHERE { SERVICE <endpoint> { ?s ?p ?o } }",
      m::SelectQuery(m::AsteriskSelect(),
                     m::GraphPattern(m::Service(
                         TripleComponent::Iri::fromIriref("<endpoint>"),
                         {Var{"?s"}, Var{"?p"}, Var{"?o"}}, "{ ?s ?p ?o }",
                         "PREFIX doof: <http://doof.org/>"))));

  // Tests around DESCRIBE.
  {
    // The tested DESCRIBE queries all describe `<x>`, `?y`, and `<z>`.
    using Resources = std::vector<parsedQuery::Describe::VarOrIri>;
    auto Iri = [](const auto& x) {
      return TripleComponent::Iri::fromIriref(x);
    };
    Resources xyz{Iri("<x>"), Var{"?y"}, Iri("<z>")};

    // A matcher for `?y <is-a> ?v`.
    auto graphPatternMatcher =
        m::GraphPattern(m::Triples({{Var{"?y"}, "<is-a>", Var{"?v"}}}));

    // A matcher for the subquery `SELECT ?y { ?y <is-a> ?v }`, which we will
    // use to compute the values for `?y` that are to be described.
    auto selectQueryMatcher1 =
        m::SelectQuery(m::Select({Var{"?y"}}), graphPatternMatcher);

    // DESCRIBE with neither FROM nor FROM NAMED clauses.
    expectQuery("DESCRIBE <x> ?y <z> { ?y <is-a> ?v }",
                m::DescribeQuery(m::Describe(xyz, {}, selectQueryMatcher1)));

    // `DESCRIBE *` query that is equivalent to `DESCRIBE <x> ?y <z> { ... }`.
    auto selectQueryMatcher2 =
        m::SelectQuery(m::Select({Var{"?y"}, Var{"?v"}}), graphPatternMatcher);
    Resources yv{Var{"?y"}, Var{"?v"}};
    expectQuery("DESCRIBE * { ?y <is-a> ?v }",
                m::DescribeQuery(m::Describe(yv, {}, selectQueryMatcher2)));

    // DESCRIBE with FROM and FROM NAMED clauses.
    //
    // NOTE: The clauses are relevant *both* for the retrieval of the resources
    // to describe (the `Id`s matching `?y`), as well as for computing the
    // triples for each of these resources.
    using Graphs = ScanSpecificationAsTripleComponent::Graphs;
    Graphs expectedDefaultGraphs;
    Graphs expectedNamedGraphs;
    expectedDefaultGraphs.emplace({Iri("<default-graph>")});
    expectedNamedGraphs.emplace({Iri("<named-graph>")});
    parsedQuery::DatasetClauses expectedClauses{expectedDefaultGraphs,
                                                expectedNamedGraphs};
    expectQuery(
        "DESCRIBE <x> ?y <z> FROM <default-graph> FROM NAMED <named-graph>",
        m::DescribeQuery(m::Describe(xyz, expectedClauses, ::testing::_),
                         expectedDefaultGraphs, expectedNamedGraphs));
  }

  // Test the various places where warnings are added in a query.
  expectQuery("SELECT ?x {} GROUP BY ?x ORDER BY ?y",
              m::WarningsOfParsedQuery({"?x was used by GROUP BY",
                                        "?y was used in an ORDER BY clause"}));
  expectQuery("SELECT * { BIND (?a as ?b) }",
              m::WarningsOfParsedQuery(
                  {"?a was used in the expression of a BIND clause"}));
  expectQuery("SELECT * { } ORDER BY ?s",
              m::WarningsOfParsedQuery({"?s was used by ORDER BY"}));

  // Now test the same queries with exceptions instead of warnings.
  RuntimeParameters().set<"throw-on-unbound-variables">(true);
  expectQueryFails("SELECT ?x {} GROUP BY ?x",
                   contains("?x was used by GROUP BY"));
  expectQueryFails("SELECT * { BIND (?a as ?b) }",
                   contains("?a was used in the expression of a BIND clause"));
  expectQueryFails("SELECT * { } ORDER BY ?s",
                   contains("?s was used by ORDER BY"));

  // Revert this (global) setting to its original value.
  RuntimeParameters().set<"throw-on-unbound-variables">(false);
}

// ___________________________________________________________________________
TEST(SparqlParser, primaryExpression) {
  using namespace sparqlExpression;
  using namespace m::builtInCall;
  auto expectPrimaryExpression =
      ExpectCompleteParse<&Parser::primaryExpression>{};
  auto expectFails = ExpectParseFails<&Parser::primaryExpression>{};

  expectPrimaryExpression("<x>", matchLiteralExpression(iri("<x>")));
  expectPrimaryExpression("\"x\"@en",
                          matchLiteralExpression(lit("\"x\"", "@en")));
  expectPrimaryExpression("27", matchLiteralExpression(IntId(27)));
}

// ___________________________________________________________________________
TEST(SparqlParser, builtInCall) {
  using namespace sparqlExpression;
  using namespace m::builtInCall;
  auto expectBuiltInCall = ExpectCompleteParse<&Parser::builtInCall>{};
  auto expectFails = ExpectParseFails<&Parser::builtInCall>{};
  expectBuiltInCall("StrLEN(?x)", matchUnary(&makeStrlenExpression));
  expectBuiltInCall("ucaSe(?x)", matchUnary(&makeUppercaseExpression));
  expectBuiltInCall("lCase(?x)", matchUnary(&makeLowercaseExpression));
  expectBuiltInCall("StR(?x)", matchUnary(&makeStrExpression));
  expectBuiltInCall(
      "iRI(?x)",
      matchNaryWithChildrenMatchers(
          &makeIriOrUriExpression, variableExpressionMatcher(Variable{"?x"}),
          matchLiteralExpression(ad_utility::triple_component::Iri{})));
  expectBuiltInCall(
      "uRI(?x)",
      matchNaryWithChildrenMatchers(
          &makeIriOrUriExpression, variableExpressionMatcher(Variable{"?x"}),
          matchLiteralExpression(ad_utility::triple_component::Iri{})));
  expectBuiltInCall("year(?x)", matchUnary(&makeYearExpression));
  expectBuiltInCall("month(?x)", matchUnary(&makeMonthExpression));
  expectBuiltInCall("tz(?x)", matchUnary(&makeTimezoneStrExpression));
  expectBuiltInCall("timezone(?x)", matchUnary(&makeTimezoneExpression));
  expectBuiltInCall("day(?x)", matchUnary(&makeDayExpression));
  expectBuiltInCall("NOW()", matchPtr<NowDatetimeExpression>());
  expectBuiltInCall("hours(?x)", matchUnary(&makeHoursExpression));
  expectBuiltInCall("minutes(?x)", matchUnary(&makeMinutesExpression));
  expectBuiltInCall("seconds(?x)", matchUnary(&makeSecondsExpression));
  expectBuiltInCall("abs(?x)", matchUnary(&makeAbsExpression));
  expectBuiltInCall("ceil(?x)", matchUnary(&makeCeilExpression));
  expectBuiltInCall("floor(?x)", matchUnary(&makeFloorExpression));
  expectBuiltInCall("round(?x)", matchUnary(&makeRoundExpression));
  expectBuiltInCall("ISIRI(?x)", matchUnary(&makeIsIriExpression));
  expectBuiltInCall("ISUri(?x)", matchUnary(&makeIsIriExpression));
  expectBuiltInCall("ISBLANK(?x)", matchUnary(&makeIsBlankExpression));
  expectBuiltInCall("ISLITERAL(?x)", matchUnary(&makeIsLiteralExpression));
  expectBuiltInCall("ISNUMERIC(?x)", matchUnary(&makeIsNumericExpression));
  expectBuiltInCall("DATATYPE(?x)", matchUnary(&makeDatatypeExpression));
  expectBuiltInCall("BOUND(?x)", matchUnary(&makeBoundExpression));
  expectBuiltInCall("RAND()", matchPtr<RandomExpression>());
  expectBuiltInCall("STRUUID()", matchPtr<StrUuidExpression>());
  expectBuiltInCall("UUID()", matchPtr<UuidExpression>());
  expectBuiltInCall("COALESCE(?x)", matchUnary(makeCoalesceExpressionVariadic));
  expectBuiltInCall("COALESCE()", matchNary(makeCoalesceExpressionVariadic));
  expectBuiltInCall("COALESCE(?x, ?y, ?z)",
                    matchNary(makeCoalesceExpressionVariadic, Var{"?x"},
                              Var{"?y"}, Var{"?z"}));
  expectBuiltInCall("CONCAT(?x)", matchUnary(makeConcatExpressionVariadic));
  expectBuiltInCall("concaT()", matchNary(makeConcatExpressionVariadic));
  expectBuiltInCall(
      "concat(?x, ?y, ?z)",
      matchNary(makeConcatExpressionVariadic, Var{"?x"}, Var{"?y"}, Var{"?z"}));

  auto makeReplaceExpressionThreeArgs = [](auto&& arg0, auto&& arg1,
                                           auto&& arg2) {
    return makeReplaceExpression(AD_FWD(arg0), AD_FWD(arg1), AD_FWD(arg2),
                                 nullptr);
  };

  expectBuiltInCall("replace(?x, ?y, ?z)",
                    matchNary(makeReplaceExpressionThreeArgs, Var{"?x"},
                              Var{"?y"}, Var{"?z"}));
  expectBuiltInCall(
      "replace(?x, ?y, ?z, \"imsU\")",
      matchNaryWithChildrenMatchers(
          makeReplaceExpressionThreeArgs, variableExpressionMatcher(Var{"?x"}),
          matchNaryWithChildrenMatchers(
              &makeMergeRegexPatternAndFlagsExpression,
              variableExpressionMatcher(Var{"?y"}),
              matchLiteralExpression(lit("imsU"))),
          variableExpressionMatcher(Var{"?z"})));
  expectBuiltInCall("IF(?a, ?h, ?c)", matchNary(&makeIfExpression, Var{"?a"},
                                                Var{"?h"}, Var{"?c"}));
  expectBuiltInCall("LANG(?x)", matchUnary(&makeLangExpression));
  expectFails("LANGMATCHES()");
  expectFails("LANGMATCHES(?x)");

  expectBuiltInCall("LANGMATCHES(?x, ?y)", matchNary(&makeLangMatchesExpression,
                                                     Var{"?x"}, Var{"?y"}));
  expectFails("STRDT()");
  expectFails("STRDT(?x)");
  expectBuiltInCall("STRDT(?x, ?y)",
                    matchNary(&makeStrIriDtExpression, Var{"?x"}, Var{"?y"}));
  expectBuiltInCall(
      "STRDT(?x, <http://example/romanNumeral>)",
      matchNaryWithChildrenMatchers(
          &makeStrIriDtExpression, variableExpressionMatcher(Var{"?x"}),
          matchLiteralExpression(iri("<http://example/romanNumeral>"))));

  expectFails("STRLANG()");
  expectFails("STRALANG(?x)");
  expectBuiltInCall("STRLANG(?x, ?y)",
                    matchNary(&makeStrLangTagExpression, Var{"?x"}, Var{"?y"}));
  expectBuiltInCall(
      "STRLANG(?x, \"en\")",
      matchNaryWithChildrenMatchers(&makeStrLangTagExpression,
                                    variableExpressionMatcher(Var{"?x"}),
                                    matchLiteralExpression(lit("en"))));

  // The following three cases delegate to a separate parsing function, so we
  // only perform rather simple checks.
  expectBuiltInCall("COUNT(?x)", matchPtr<CountExpression>());
  auto makeRegexExpressionTwoArgs = [](auto&& arg0, auto&& arg1) {
    return makeRegexExpression(AD_FWD(arg0), AD_FWD(arg1), nullptr);
  };
  expectBuiltInCall(
      "regex(?x, \"ab\")",
      matchNaryWithChildrenMatchers(makeRegexExpressionTwoArgs,
                                    variableExpressionMatcher(Var{"?x"}),
                                    matchLiteralExpression(lit("ab"))));
  expectBuiltInCall(
      "regex(?x, \"ab\", \"imsU\")",
      matchNaryWithChildrenMatchers(
          makeRegexExpressionTwoArgs, variableExpressionMatcher(Var{"?x"}),
          matchNaryWithChildrenMatchers(
              &makeMergeRegexPatternAndFlagsExpression,
              matchLiteralExpression(lit("ab")),
              matchLiteralExpression(lit("imsU")))));

  expectBuiltInCall("MD5(?x)", matchUnary(&makeMD5Expression));
  expectBuiltInCall("SHA1(?x)", matchUnary(&makeSHA1Expression));
  expectBuiltInCall("SHA256(?x)", matchUnary(&makeSHA256Expression));
  expectBuiltInCall("SHA384(?x)", matchUnary(&makeSHA384Expression));
  expectBuiltInCall("SHA512(?x)", matchUnary(&makeSHA512Expression));

  expectBuiltInCall("encode_for_uri(?x)",
                    matchUnary(&makeEncodeForUriExpression));

  const auto& blankNodeExpression = makeUniqueBlankNodeExpression();
  const auto& reference = *blankNodeExpression;
  expectBuiltInCall(
      "bnode()", testing::Pointee(::testing::ResultOf(
                     [](const SparqlExpression& expr) -> const std::type_info& {
                       return typeid(expr);
                     },
                     Eq(std::reference_wrapper(typeid(reference))))));
  expectBuiltInCall("bnode(?x)", matchUnary(&makeBlankNodeExpression));
  // Not implemented yet
  expectFails("sameTerm(?a, ?b)");
}

TEST(SparqlParser, unaryExpression) {
  using namespace sparqlExpression;
  using namespace m::builtInCall;
  auto expectUnary = ExpectCompleteParse<&Parser::unaryExpression>{};

  expectUnary("-?x", matchUnary(&makeUnaryMinusExpression));
  expectUnary("!?x", matchUnary(&makeUnaryNegateExpression));
}

TEST(SparqlParser, multiplicativeExpression) {
  using namespace sparqlExpression;
  using namespace m::builtInCall;
  Variable x{"?x"};
  Variable y{"?y"};
  Variable z{"?z"};
  auto expectMultiplicative =
      ExpectCompleteParse<&Parser::multiplicativeExpression>{};
  expectMultiplicative("?x * ?y", matchNary(&makeMultiplyExpression, x, y));
  expectMultiplicative("?y / ?x", matchNary(&makeDivideExpression, y, x));
  expectMultiplicative(
      "?z * ?y / abs(?x)",
      matchNaryWithChildrenMatchers(&makeDivideExpression,
                                    matchNary(&makeMultiplyExpression, z, y),
                                    matchUnary(&makeAbsExpression)));
  expectMultiplicative(
      "?y / ?z * abs(?x)",
      matchNaryWithChildrenMatchers(&makeMultiplyExpression,
                                    matchNary(&makeDivideExpression, y, z),
                                    matchUnary(&makeAbsExpression)));
}

TEST(SparqlParser, relationalExpression) {
  Variable x{"?x"};
  Variable y{"?y"};
  Variable z{"?z"};
  using namespace sparqlExpression;
  using namespace m::builtInCall;
  auto expectRelational = ExpectCompleteParse<&Parser::relationalExpression>{};
  expectRelational("?x IN (?y, ?z)",
                   matchPtrWithVariables<InExpression>(x, y, z));
  expectRelational("?x NOT IN (?y, ?z)",
                   matchNaryWithChildrenMatchers(
                       &makeUnaryNegateExpression,
                       matchPtrWithVariables<InExpression>(x, y, z)));
  // TODO<joka921> Technically the other relational expressions (=, <, >, etc.)
  // are also untested.
}

// Return a matcher for an `OperatorAndExpression`.
::testing::Matcher<const SparqlQleverVisitor::OperatorAndExpression&>
matchOperatorAndExpression(
    SparqlQleverVisitor::Operator op,
    const ::testing::Matcher<const sparqlExpression::SparqlExpression::Ptr&>&
        expressionMatcher) {
  using OpAndExp = SparqlQleverVisitor::OperatorAndExpression;
  return ::testing::AllOf(AD_FIELD(OpAndExp, operator_, ::testing::Eq(op)),
                          AD_FIELD(OpAndExp, expression_, expressionMatcher));
}

TEST(SparqlParser, multiplicativeExpressionLeadingSignButNoSpaceContext) {
  using namespace sparqlExpression;
  using namespace m::builtInCall;
  Variable x{"?x"};
  Variable y{"?y"};
  Variable z{"?z"};
  using Op = SparqlQleverVisitor::Operator;
  auto expectMultiplicative = ExpectCompleteParse<
      &Parser::multiplicativeExpressionWithLeadingSignButNoSpace>{};
  auto matchVariableExpression = [](Variable var) {
    return matchPtr<VariableExpression>(
        AD_PROPERTY(VariableExpression, value, ::testing::Eq(var)));
  };
  auto matchIdExpression = [](Id id) {
    return matchPtr<IdExpression>(
        AD_PROPERTY(IdExpression, value, ::testing::Eq(id)));
  };

  expectMultiplicative("-3 * ?y",
                       matchOperatorAndExpression(
                           Op::Minus, matchNaryWithChildrenMatchers(
                                          &makeMultiplyExpression,
                                          matchIdExpression(Id::makeFromInt(3)),
                                          matchVariableExpression(y))));
  expectMultiplicative(
      "-3.7 / ?y",
      matchOperatorAndExpression(
          Op::Minus,
          matchNaryWithChildrenMatchers(
              &makeDivideExpression, matchIdExpression(Id::makeFromDouble(3.7)),
              matchVariableExpression(y))));

  expectMultiplicative("+5 * ?y",
                       matchOperatorAndExpression(
                           Op::Plus, matchNaryWithChildrenMatchers(
                                         &makeMultiplyExpression,
                                         matchIdExpression(Id::makeFromInt(5)),
                                         matchVariableExpression(y))));
  expectMultiplicative(
      "+3.9 / ?y", matchOperatorAndExpression(
                       Op::Plus, matchNaryWithChildrenMatchers(
                                     &makeDivideExpression,
                                     matchIdExpression(Id::makeFromDouble(3.9)),
                                     matchVariableExpression(y))));
  expectMultiplicative(
      "-3.2 / abs(?x) * ?y",
      matchOperatorAndExpression(
          Op::Minus, matchNaryWithChildrenMatchers(
                         &makeMultiplyExpression,
                         matchNaryWithChildrenMatchers(
                             &makeDivideExpression,
                             matchIdExpression(Id::makeFromDouble(3.2)),
                             matchUnary(&makeAbsExpression)),
                         matchVariableExpression(y))));
}

TEST(SparqlParser, FunctionCall) {
  using namespace sparqlExpression;
  using namespace m::builtInCall;
  auto expectFunctionCall = ExpectCompleteParse<&Parser::functionCall>{};
  auto expectFunctionCallFails = ExpectParseFails<&Parser::functionCall>{};
  // These prefixes are currently stored without the leading "<", so we have to
  // manually add it when constructing parser inputs.
  auto geof = absl::StrCat("<", GEOF_PREFIX.second);
  auto math = absl::StrCat("<", MATH_PREFIX.second);
  auto xsd = absl::StrCat("<", XSD_PREFIX.second);
  auto ql = absl::StrCat("<", QL_PREFIX.second);

  // Correct function calls. Check that the parser picks the correct expression.
  expectFunctionCall(absl::StrCat(geof, "latitude>(?x)"),
                     matchUnary(&makeLatitudeExpression));
  expectFunctionCall(absl::StrCat(geof, "longitude>(?x)"),
                     matchUnary(&makeLongitudeExpression));
  expectFunctionCall(absl::StrCat(geof, "centroid>(?x)"),
                     matchUnary(&makeCentroidExpression));
  expectFunctionCall(absl::StrCat(ql, "isGeoPoint>(?x)"),
                     matchUnary(&makeIsGeoPointExpression));
  expectFunctionCall(absl::StrCat(geof, "envelope>(?x)"),
                     matchUnary(&makeEnvelopeExpression));

  // The different distance functions:
  expectFunctionCall(
      absl::StrCat(geof, "metricDistance>(?a, ?b)"),
      matchNary(&makeMetricDistExpression, Variable{"?a"}, Variable{"?b"}));
  // Compatibility version of geof:distance with two arguments
  expectFunctionCall(
      absl::StrCat(geof, "distance>(?a, ?b)"),
      matchNary(&makeDistExpression, Variable{"?a"}, Variable{"?b"}));
  // geof:distance with IRI as unit in third argument
  expectFunctionCall(
      absl::StrCat(geof, "distance>(?a, ?b, <http://qudt.org/vocab/unit/M>)"),
      matchNaryWithChildrenMatchers(
          &makeDistWithUnitExpression,
          variableExpressionMatcher(Variable{"?a"}),
          variableExpressionMatcher(Variable{"?b"}),
          matchLiteralExpression<ad_utility::triple_component::Iri>(
              ad_utility::triple_component::Iri::fromIriref(
                  "<http://qudt.org/vocab/unit/M>"))));

  // geof:distance with xsd:anyURI literal as unit in third argument
  expectFunctionCall(
      absl::StrCat(geof,
                   "distance>(?a, ?b, "
                   "\"http://qudt.org/vocab/unit/M\"^^<http://www.w3.org/2001/"
                   "XMLSchema#anyURI>)"),
      matchNaryWithChildrenMatchers(
          &makeDistWithUnitExpression,
          variableExpressionMatcher(Variable{"?a"}),
          variableExpressionMatcher(Variable{"?b"}),
          matchLiteralExpression<ad_utility::triple_component::Literal>(
              ad_utility::triple_component::Literal::fromStringRepresentation(
                  "\"http://qudt.org/vocab/unit/M\"^^<http://www.w3.org/2001/"
                  "XMLSchema#anyURI>"))));

  // geof:distance with variable as unit in third argument
  expectFunctionCall(absl::StrCat(geof, "distance>(?a, ?b, ?unit)"),
                     matchNaryWithChildrenMatchers(
                         &makeDistWithUnitExpression,
                         variableExpressionMatcher(Variable{"?a"}),
                         variableExpressionMatcher(Variable{"?b"}),
                         variableExpressionMatcher(Variable{"?unit"})));

  // Math functions
  expectFunctionCall(absl::StrCat(math, "log>(?x)"),
                     matchUnary(&makeLogExpression));
  expectFunctionCall(absl::StrCat(math, "exp>(?x)"),
                     matchUnary(&makeExpExpression));
  expectFunctionCall(absl::StrCat(math, "sqrt>(?x)"),
                     matchUnary(&makeSqrtExpression));
  expectFunctionCall(absl::StrCat(math, "sin>(?x)"),
                     matchUnary(&makeSinExpression));
  expectFunctionCall(absl::StrCat(math, "cos>(?x)"),
                     matchUnary(&makeCosExpression));
  expectFunctionCall(absl::StrCat(math, "tan>(?x)"),
                     matchUnary(&makeTanExpression));
  expectFunctionCall(
      absl::StrCat(math, "pow>(?a, ?b)"),
      matchNary(&makePowExpression, Variable{"?a"}, Variable{"?b"}));
  expectFunctionCall(absl::StrCat(xsd, "int>(?x)"),
                     matchUnary(&makeConvertToIntExpression));
  expectFunctionCall(absl::StrCat(xsd, "integer>(?x)"),
                     matchUnary(&makeConvertToIntExpression));
  expectFunctionCall(absl::StrCat(xsd, "double>(?x)"),
                     matchUnary(&makeConvertToDoubleExpression));
  expectFunctionCall(absl::StrCat(xsd, "float>(?x)"),
                     matchUnary(&makeConvertToDoubleExpression));
  expectFunctionCall(absl::StrCat(xsd, "decimal>(?x)"),
                     matchUnary(&makeConvertToDecimalExpression));
  expectFunctionCall(absl::StrCat(xsd, "boolean>(?x)"),
                     matchUnary(&makeConvertToBooleanExpression));
  expectFunctionCall(absl::StrCat(xsd, "date>(?x)"),
                     matchUnary(&makeConvertToDateExpression));
  expectFunctionCall(absl::StrCat(xsd, "dateTime>(?x)"),
                     matchUnary(&makeConvertToDateTimeExpression));

  expectFunctionCall(absl::StrCat(xsd, "string>(?x)"),
                     matchUnary(&makeConvertToStringExpression));

  // Wrong number of arguments.
  expectFunctionCallFails(absl::StrCat(geof, "distance>(?a)"));
  expectFunctionCallFails(absl::StrCat(geof, "distance>()"));
  expectFunctionCallFails(absl::StrCat(geof, "distance>(?a, ?b, ?c, ?d)"));
  expectFunctionCallFails(absl::StrCat(geof, "metricDistance>(?a)"));
  expectFunctionCallFails(absl::StrCat(geof, "metricDistance>(?a, ?b, ?c)"));
  expectFunctionCallFails(absl::StrCat(geof, "centroid>(?a, ?b)"));
  expectFunctionCallFails(absl::StrCat(geof, "centroid>()"));
  expectFunctionCallFails(absl::StrCat(geof, "centroid>(?a, ?b, ?c)"));
  expectFunctionCallFails(absl::StrCat(xsd, "date>(?varYear, ?varMonth)"));
  expectFunctionCallFails(absl::StrCat(xsd, "dateTime>(?varYear, ?varMonth)"));
  expectFunctionCallFails(absl::StrCat(geof, "envelope>()"));
  expectFunctionCallFails(absl::StrCat(geof, "envelope>(?a, ?b)"));
  expectFunctionCallFails(absl::StrCat(geof, "envelope>(?a, ?b, ?c)"));

  // Unknown function with `geof:`, `math:`, `xsd:`, or `ql` prefix.
  expectFunctionCallFails(absl::StrCat(geof, "nada>(?x)"));
  expectFunctionCallFails(absl::StrCat(math, "nada>(?x)"));
  expectFunctionCallFails(absl::StrCat(xsd, "nada>(?x)"));
  expectFunctionCallFails(absl::StrCat(ql, "nada>(?x)"));

  // Prefix for which no function is known.
  std::string prefixNexistepas = "<http://nexiste.pas/";
  expectFunctionCallFails(absl::StrCat(prefixNexistepas, "nada>(?x)"));

  // Check that arbitrary nonexisting functions with a single argument silently
  // return an `IdExpression(UNDEF)` in the syntax test mode.
  auto cleanup = setRuntimeParameterForTest<"syntax-test-mode">(true);
  expectFunctionCall(
      absl::StrCat(prefixNexistepas, "nada>(?x)"),
      matchPtr<IdExpression>(AD_PROPERTY(IdExpression, value,
                                         ::testing::Eq(Id::makeUndefined()))));
}

// ______________________________________________________________________________
TEST(SparqlParser, substringExpression) {
  using namespace sparqlExpression;
  using namespace m::builtInCall;
  using V = Variable;
  auto expectBuiltInCall = ExpectCompleteParse<&Parser::builtInCall>{};
  auto expectBuiltInCallFails = ExpectParseFails<&Parser::builtInCall>{};
  expectBuiltInCall("SUBSTR(?x, ?y, ?z)", matchNary(&makeSubstrExpression,
                                                    V{"?x"}, V{"?y"}, V{"?z"}));
  // Note: The large number (the default value for the length, which is
  // automatically truncated) is the largest integer that is representable by
  // QLever. Should this ever change, then this test has to be changed
  // accordingly.
  expectBuiltInCall(
      "SUBSTR(?x, 7)",
      matchNaryWithChildrenMatchers(&makeSubstrExpression,
                                    variableExpressionMatcher(V{"?x"}),
                                    idExpressionMatcher(IntId(7)),
                                    idExpressionMatcher(IntId(Id::maxInt))));
  // Too few arguments
  expectBuiltInCallFails("SUBSTR(?x)");
  // Too many arguments
  expectBuiltInCallFails("SUBSTR(?x, 3, 8, 12)");
}

// _________________________________________________________
TEST(SparqlParser, binaryStringExpressions) {
  using namespace sparqlExpression;
  using namespace m::builtInCall;
  using V = Variable;
  auto expectBuiltInCall = ExpectCompleteParse<&Parser::builtInCall>{};
  auto expectBuiltInCallFails = ExpectParseFails<&Parser::builtInCall>{};

  auto makeMatcher = [](auto function) {
    return matchNary(function, V{"?x"}, V{"?y"});
  };

  expectBuiltInCall("STRSTARTS(?x, ?y)", makeMatcher(&makeStrStartsExpression));
  expectBuiltInCall("STRENDS(?x, ?y)", makeMatcher(&makeStrEndsExpression));
  expectBuiltInCall("CONTAINS(?x, ?y)", makeMatcher(&makeContainsExpression));
  expectBuiltInCall("STRAFTER(?x, ?y)", makeMatcher(&makeStrAfterExpression));
  expectBuiltInCall("STRBEFORE(?x, ?y)", makeMatcher(&makeStrBeforeExpression));
}

// _____________________________________________________________________________
TEST(SparqlParser, Exists) {
  auto expectBuiltInCall = ExpectCompleteParse<&Parser::builtInCall>{};

  // A matcher that matches the query `SELECT * { ?x <bar> ?foo }`, where the
  // FROM and FROM NAMED clauses can be specified as arguments.
  using Graphs = ScanSpecificationAsTripleComponent::Graphs;
  auto selectABarFooMatcher =
      [](Graphs defaultGraphs = std::nullopt, Graphs namedGraphs = std::nullopt,
         const std::optional<std::vector<std::string>>& variables =
             std::nullopt) {
        auto selectMatcher = variables.has_value()
                                 ? m::VariablesSelect(variables.value())
                                 : AllOf(m::AsteriskSelect(),
                                         m::VariablesSelect({"?a", "?foo"}));
        return m::SelectQuery(
            selectMatcher,
            m::GraphPattern(m::Triples({{Var{"?a"}, "<bar>", Var{"?foo"}}})),
            defaultGraphs, namedGraphs);
      };

  expectBuiltInCall("EXISTS {?a <bar> ?foo}",
                    m::Exists(selectABarFooMatcher()));
  expectBuiltInCall("NOT EXISTS {?a <bar> ?foo}",
                    m::NotExists(selectABarFooMatcher()));

  Graphs defaultGraphs{ad_utility::HashSet<TripleComponent>{iri("<blubb>")}};
  Graphs namedGraphs{ad_utility::HashSet<TripleComponent>{iri("<blabb>")}};

  // Now run the same tests, but with non-empty dataset clauses, that have to be
  // propagated to the `ParsedQuery` stored inside the `ExistsExpression`.
  ParsedQuery::DatasetClauses datasetClauses{defaultGraphs, namedGraphs};
  expectBuiltInCall("EXISTS {?a <bar> ?foo}",
                    m::Exists(selectABarFooMatcher()));
  expectBuiltInCall("NOT EXISTS {?a <bar> ?foo}",
                    m::NotExists(selectABarFooMatcher()));

  expectBuiltInCall("EXISTS {?a <bar> ?foo}",
                    m::Exists(selectABarFooMatcher(defaultGraphs, namedGraphs)),
                    datasetClauses);
  expectBuiltInCall(
      "NOT EXISTS {?a <bar> ?foo}",
      m::NotExists(selectABarFooMatcher(defaultGraphs, namedGraphs)),
      datasetClauses);

  auto expectGroupGraphPattern =
      ExpectCompleteParse<&Parser::groupGraphPattern>{};
  expectGroupGraphPattern("{ ?a ?b ?c . FILTER EXISTS {?a <bar> ?foo} }",
                          m::ContainsExistsFilter(selectABarFooMatcher(
                              std::nullopt, std::nullopt, {{"?a"}})));
  expectGroupGraphPattern("{ ?a ?b ?c . FILTER NOT EXISTS {?a <bar> ?foo} }",
                          m::ContainsExistsFilter(selectABarFooMatcher(
                              std::nullopt, std::nullopt, {{"?a"}})));
  expectGroupGraphPattern("{ FILTER EXISTS {?a <bar> ?foo} . ?a ?b ?c }",
                          m::ContainsExistsFilter(selectABarFooMatcher(
                              std::nullopt, std::nullopt, {{"?a"}})));
  expectGroupGraphPattern("{ FILTER NOT EXISTS {?a <bar> ?foo} . ?a ?b ?c }",
                          m::ContainsExistsFilter(selectABarFooMatcher(
                              std::nullopt, std::nullopt, {{"?a"}})));

  auto doesNotBindExists = [&]() {
    auto innerMatcher = m::ContainsExistsFilter(selectABarFooMatcher(
        std::nullopt, std::nullopt, std::vector<std::string>{}));
    using parsedQuery::GroupGraphPattern;
    return AD_FIELD(parsedQuery::GraphPattern, _graphPatterns,
                    ::testing::ElementsAre(
                        ::testing::VariantWith<GroupGraphPattern>(
                            AD_FIELD(GroupGraphPattern, _child, innerMatcher)),
                        ::testing::_));
  };

  expectGroupGraphPattern(
      "{ { FILTER EXISTS {?a <bar> ?foo} . ?d ?e ?f } . ?a ?b ?c }",
      doesNotBindExists());
  expectGroupGraphPattern(
      "{ { FILTER NOT EXISTS {?a <bar> ?foo} . ?d ?e ?f  } ?a ?b ?c }",
      doesNotBindExists());
}

namespace aggregateTestHelpers {
using namespace sparqlExpression;

// Return a matcher that checks whether a given `SparqlExpression::Ptr` actually
// points to an `AggregateExpr`, that the distinctness and the child variable of
// the aggregate expression match, and that the `AggregateExpr`(via dynamic
// cast) matches all the `additionalMatchers`.
template <typename AggregateExpr>
::testing::Matcher<const SparqlExpression::Ptr&> matchAggregate(
    bool distinct, const Variable& child, const auto&... additionalMatchers) {
  using namespace ::testing;
  using namespace m::builtInCall;
  using Exp = SparqlExpression;

  auto innerMatcher = [&]() -> Matcher<const AggregateExpr&> {
    if constexpr (sizeof...(additionalMatchers) > 0) {
      return AllOf(additionalMatchers...);
    } else {
      return ::testing::_;
    }
  }();
  using enum SparqlExpression::AggregateStatus;
  auto aggregateStatus = distinct ? DistinctAggregate : NonDistinctAggregate;
  return Pointee(AllOf(
      AD_PROPERTY(Exp, isAggregate, Eq(aggregateStatus)),
      AD_PROPERTY(Exp, children, ElementsAre(variableExpressionMatcher(child))),
      WhenDynamicCastTo<const AggregateExpr&>(innerMatcher)));
}

// Return a matcher that checks whether a given `SparqlExpression::Ptr` actually
// points to an `AggregateExpr` and that the distinctness of the aggregate
// expression matches. It does not check the child. This is required to test
// aggregates that implicitly replace their child, like `StdevExpression`.
template <typename AggregateExpr>
::testing::Matcher<const SparqlExpression::Ptr&> matchAggregateWithoutChild(
    bool distinct) {
  using namespace ::testing;
  using namespace m::builtInCall;
  using Exp = SparqlExpression;

  using enum SparqlExpression::AggregateStatus;
  auto aggregateStatus = distinct ? DistinctAggregate : NonDistinctAggregate;
  return Pointee(AllOf(AD_PROPERTY(Exp, isAggregate, Eq(aggregateStatus)),
                       WhenDynamicCastTo<const AggregateExpr&>(testing::_)));
}
}  // namespace aggregateTestHelpers

// ___________________________________________________________
TEST(SparqlParser, aggregateExpressions) {
  using namespace sparqlExpression;
  using namespace m::builtInCall;
  using namespace aggregateTestHelpers;
  using V = Variable;
  auto expectAggregate = ExpectCompleteParse<&Parser::aggregate>{};
  auto expectAggregateFails = ExpectParseFails<&Parser::aggregate>{};

  // For the `COUNT *` expression we have completely hidden the type. So we need
  // to match it via RTTI.
  auto typeIdLambda = [](const auto& ptr) {
    return std::type_index{typeid(ptr)};
  };
  auto typeIdxCountStar = typeIdLambda(*makeCountStarExpression(true));

  // A matcher that matches a `COUNT *` expression with the given distinctness.
  auto matchCountStar =
      [&typeIdLambda, typeIdxCountStar](
          bool distinct) -> ::testing::Matcher<const SparqlExpression::Ptr&> {
    using namespace ::testing;
    using enum SparqlExpression::AggregateStatus;
    auto aggregateStatus = distinct ? DistinctAggregate : NonDistinctAggregate;
    return Pointee(
        AllOf(AD_PROPERTY(SparqlExpression, isAggregate, Eq(aggregateStatus)),
              ResultOf(typeIdLambda, Eq(typeIdxCountStar))));
  };

  expectAggregate("COUNT(*)", matchCountStar(false));
  expectAggregate("COUNT(DISTINCT *)", matchCountStar(true));

  expectAggregate("SAMPLE(?x)",
                  matchAggregate<SampleExpression>(false, V{"?x"}));
  expectAggregate("SAMPLE(DISTINCT ?x)",
                  matchAggregate<SampleExpression>(false, V{"?x"}));

  expectAggregate("Min(?x)", matchAggregate<MinExpression>(false, V{"?x"}));
  expectAggregate("Min(DISTINCT ?x)",
                  matchAggregate<MinExpression>(true, V{"?x"}));

  expectAggregate("Max(?x)", matchAggregate<MaxExpression>(false, V{"?x"}));
  expectAggregate("Max(DISTINCT ?x)",
                  matchAggregate<MaxExpression>(true, V{"?x"}));

  expectAggregate("Count(?x)", matchAggregate<CountExpression>(false, V{"?x"}));
  expectAggregate("Count(DISTINCT ?x)",
                  matchAggregate<CountExpression>(true, V{"?x"}));

  expectAggregate("Avg(?x)", matchAggregate<AvgExpression>(false, V{"?x"}));
  expectAggregate("Avg(DISTINCT ?x)",
                  matchAggregate<AvgExpression>(true, V{"?x"}));

  // A matcher for the separator of `GROUP_CONCAT`.
  auto separator = [](const std::string& sep) {
    return AD_PROPERTY(GroupConcatExpression, getSeparator, Eq(sep));
  };
  expectAggregate("GROUP_CONCAT(?x)", matchAggregate<GroupConcatExpression>(
                                          false, V{"?x"}, separator(" ")));
  expectAggregate(
      "group_concat(DISTINCT ?x)",
      matchAggregate<GroupConcatExpression>(true, V{"?x"}, separator(" ")));

  expectAggregate(
      "GROUP_CONCAT(?x; SEPARATOR= \";\")",
      matchAggregate<GroupConcatExpression>(false, V{"?x"}, separator(";")));
  expectAggregate(
      "group_concat(DISTINCT ?x; SEPARATOR=\";\")",
      matchAggregate<GroupConcatExpression>(true, V{"?x"}, separator(";")));

  // The STDEV expression
  // Here we don't match the child, because StdevExpression replaces it with a
  // DeviationExpression.
  expectAggregate("STDEV(?x)",
                  matchAggregateWithoutChild<StdevExpression>(false));
  expectAggregate("stdev(?x)",
                  matchAggregateWithoutChild<StdevExpression>(false));
  // A distinct stdev is probably not very useful, but should be possible anyway
  expectAggregate("STDEV(DISTINCT ?x)",
                  matchAggregateWithoutChild<StdevExpression>(true));
}

TEST(SparqlParser, Quads) {
  auto expectQuads = ExpectCompleteParse<&Parser::quads>{defaultPrefixMap};
  auto expectQuadsFails = ExpectParseFails<&Parser::quads>{};
  auto Iri = [](std::string_view stringWithBrackets) {
    return TripleComponent::Iri::fromIriref(stringWithBrackets);
  };

  expectQuads("?a <b> <c>",
              m::Quads({{Var("?a"), ::Iri("<b>"), ::Iri("<c>")}}, {}));
  expectQuads("GRAPH <foo> { ?a <b> <c> }",
              m::Quads({}, {{Iri("<foo>"),
                             {{Var("?a"), ::Iri("<b>"), ::Iri("<c>")}}}}));
  expectQuads(
      "GRAPH <foo> { ?a <b> <c> } GRAPH <bar> { <d> <e> ?f }",
      m::Quads({},
               {{Iri("<foo>"), {{Var("?a"), ::Iri("<b>"), ::Iri("<c>")}}},
                {Iri("<bar>"), {{::Iri("<d>"), ::Iri("<e>"), Var("?f")}}}}));
  expectQuads(
      "GRAPH <foo> { ?a <b> <c> } . <d> <e> <f> . <g> <h> <i> ",
      m::Quads({{::Iri("<d>"), ::Iri("<e>"), ::Iri("<f>")},
                {::Iri("<g>"), ::Iri("<h>"), ::Iri("<i>")}},
               {{Iri("<foo>"), {{Var("?a"), ::Iri("<b>"), ::Iri("<c>")}}}}));
  expectQuads(
      "GRAPH <foo> { ?a <b> <c> } . <d> <e> <f> . <g> <h> <i> GRAPH <bar> { "
      "<j> <k> <l> }",
      m::Quads({{::Iri("<d>"), ::Iri("<e>"), ::Iri("<f>")},
                {::Iri("<g>"), ::Iri("<h>"), ::Iri("<i>")}},
               {{Iri("<foo>"), {{Var("?a"), ::Iri("<b>"), ::Iri("<c>")}}},
                {Iri("<bar>"), {{::Iri("<j>"), ::Iri("<k>"), ::Iri("<l>")}}}}));
}

TEST(SparqlParser, QuadData) {
  auto expectQuadData =
      ExpectCompleteParse<&Parser::quadData>{defaultPrefixMap};
  auto expectQuadDataFails = ExpectParseFails<&Parser::quadData>{};

  expectQuadData("{ <a> <b> <c> }",
                 Quads{{{Iri("<a>"), Iri("<b>"), Iri("<c>")}}, {}});
  expectQuadDataFails("{ <a> <b> ?c }");
  expectQuadDataFails("{ <a> <b> <c> . GRAPH <foo> { <d> ?e <f> } }");
  expectQuadDataFails("{ <a> <b> <c> . ?d <e> <f> } }");
  expectQuadDataFails("{ GRAPH ?foo { <a> <b> <c> } }");
}

TEST(SparqlParser, Update) {
  auto expectUpdate_ = ExpectCompleteParse<&Parser::update>{defaultPrefixMap};
  // Automatically test all updates for their `_originalString`.
  auto expectUpdate = [&expectUpdate_](
                          const std::string& query, auto&& expected,
                          ad_utility::source_location l =
                              ad_utility::source_location::current()) {
    expectUpdate_(query,
                  testing::ElementsAre(
                      testing::AllOf(expected, m::pq::OriginalString(query))),
                  l);
  };
  auto expectUpdateFails = ExpectParseFails<&Parser::update>{};
  auto Iri = [](std::string_view stringWithBrackets) {
    return TripleComponent::Iri::fromIriref(stringWithBrackets);
  };
  auto Literal = [](std::string s) {
    return TripleComponent::Literal::fromStringRepresentation(std::move(s));
  };
  auto noGraph = std::monostate{};

  // Test the parsing of the update clause in the ParsedQuery.
  expectUpdate(
      "INSERT DATA { <a> <b> <c> }",
      m::UpdateClause(
          m::GraphUpdate({}, {{Iri("<a>"), Iri("<b>"), Iri("<c>"), noGraph}}),
          m::GraphPattern()));
  expectUpdate(
      "INSERT DATA { <a> <b> \"foo:bar\" }",
      m::UpdateClause(m::GraphUpdate({}, {{Iri("<a>"), Iri("<b>"),
                                           Literal("\"foo:bar\""), noGraph}}),
                      m::GraphPattern()));
  expectUpdate(
      "DELETE DATA { <a> <b> <c> }",
      m::UpdateClause(
          m::GraphUpdate({{Iri("<a>"), Iri("<b>"), Iri("<c>"), noGraph}}, {}),
          m::GraphPattern()));
  expectUpdate(
      "DELETE { ?a <b> <c> } WHERE { <d> <e> ?a }",
      m::UpdateClause(
          m::GraphUpdate({{Var("?a"), Iri("<b>"), Iri("<c>"), noGraph}}, {}),
          m::GraphPattern(m::Triples({{Iri("<d>"), "<e>", Var{"?a"}}}))));
  // Use variables that are not visible in the query body. Do this for all parts
  // of the quad for coverage reasons.
  expectUpdateFails("DELETE { ?a <b> <c> } WHERE { <a> ?b ?c }");
  expectUpdateFails("DELETE { <c> <d> <c> . <e> ?a <f> } WHERE { <a> ?b ?c }");
  expectUpdateFails(
      "DELETE { GRAPH <foo> { <c> <d> <c> . <e> <f> ?a } } WHERE { <a> ?b ?c "
      "}");
  expectUpdateFails("DELETE { GRAPH ?a { <c> <d> <c> } } WHERE { <a> ?b ?c }");
  expectUpdate(
      "DELETE { ?a <b> <c> } INSERT { <a> ?a <c> } WHERE { <d> <e> ?a }",
      m::UpdateClause(
          m::GraphUpdate({{Var("?a"), Iri("<b>"), Iri("<c>"), noGraph}},
                         {{Iri("<a>"), Var("?a"), Iri("<c>"), noGraph}}),
          m::GraphPattern(m::Triples({{Iri("<d>"), "<e>", Var{"?a"}}}))));
  expectUpdate(
      "DELETE WHERE { ?a <foo> ?c }",
      m::UpdateClause(
          m::GraphUpdate({{Var("?a"), Iri("<foo>"), Var("?c"), noGraph}}, {}),
          m::GraphPattern(m::Triples({{Var{"?a"}, "<foo>", Var{"?c"}}}))));
  expectUpdateFails("INSERT DATA { ?a ?b ?c }");  // Variables are not allowed
  // inside INSERT DATA.
  expectUpdate(
      "WITH <foo> DELETE { ?a ?b ?c } WHERE { ?a ?b ?c }",
      m::UpdateClause(
          m::GraphUpdate({{Var("?a"), Var("?b"), Var("?c"), Iri("<foo>")}}, {}),
          m::GraphPattern(m::Triples({{Var{"?a"}, Var{"?b"}, Var{"?c"}}})),
          m::datasetClausesMatcher(m::Graphs{TripleComponent(Iri("<foo>"))},
                                   std::nullopt)));
  expectUpdate(
      "DELETE { ?a ?b ?c } USING <foo> WHERE { ?a ?b ?c }",
      m::UpdateClause(
          m::GraphUpdate({{Var("?a"), Var("?b"), Var("?c"), noGraph}}, {}),
          m::GraphPattern(m::Triples({{Var{"?a"}, Var{"?b"}, Var{"?c"}}})),
          m::datasetClausesMatcher(m::Graphs{TripleComponent(Iri("<foo>"))},
                                   m::Graphs{})));
  expectUpdate("INSERT DATA { GRAPH <foo> { } }",
               m::UpdateClause(m::GraphUpdate({}, {}), m::GraphPattern()));
  expectUpdate("INSERT DATA { GRAPH <foo> { <a> <b> <c> } }",
               m::UpdateClause(m::GraphUpdate({}, {{Iri("<a>"), Iri("<b>"),
                                                    Iri("<c>"), Iri("<foo>")}}),
                               m::GraphPattern()));
  expectUpdateFails(
      "INSERT DATA { GRAPH ?f { } }",
      testing::HasSubstr(
          "Invalid SPARQL query: Variables (?f) are not allowed here."));
  expectUpdate(
      "DELETE { ?a <b> <c> } USING NAMED <foo> WHERE { <d> <e> ?a }",
      m::UpdateClause(
          m::GraphUpdate({{Var("?a"), Iri("<b>"), Iri("<c>"), noGraph}}, {}),
          m::GraphPattern(m::Triples({{Iri("<d>"), "<e>", Var{"?a"}}})),
          m::datasetClausesMatcher(m::Graphs{},
                                   m::Graphs{TripleComponent(Iri("<foo>"))})));
  expectUpdate(
      "WITH <foo> DELETE { ?a <b> <c> } WHERE { <d> <e> ?a }",
      m::UpdateClause(
          m::GraphUpdate({{Var("?a"), Iri("<b>"), Iri("<c>"), Iri("<foo>")}},
                         {}),
          m::GraphPattern(m::Triples({{Iri("<d>"), "<e>", Var{"?a"}}})),
          m::datasetClausesMatcher(m::Graphs{TripleComponent(Iri("<foo>"))},
                                   std::nullopt)));
  const auto insertMatcher = m::UpdateClause(
      m::GraphUpdate({}, {{Iri("<a>"), Iri("<b>"), Iri("<c>"), noGraph}}),
      m::GraphPattern());
  const auto fooInsertMatcher = m::UpdateClause(
      m::GraphUpdate(
          {}, {{Iri("<foo/a>"), Iri("<foo/b>"), Iri("<foo/c>"), noGraph}}),
      m::GraphPattern());
  const auto deleteWhereAllMatcher = m::UpdateClause(
      m::GraphUpdate({{Var("?s"), Var("?p"), Var("?o"), noGraph}}, {}),
      m::GraphPattern(m::Triples({{Var("?s"), Var{"?p"}, Var("?o")}})));
  expectUpdate("INSERT DATA { <a> <b> <c> }", insertMatcher);
  // Multiple Updates
  expectUpdate_(
      "INSERT DATA { <a> <b> <c> };",
      ElementsAre(AllOf(insertMatcher,
                        m::pq::OriginalString("INSERT DATA { <a> <b> <c> }"))));
  expectUpdate_(
      "INSERT DATA { <a> <b> <c> }; BASE <https://example.org> PREFIX foo: "
      "<foo>",
      ElementsAre(AllOf(insertMatcher,
                        m::pq::OriginalString("INSERT DATA { <a> <b> <c> }"))));
  expectUpdate_(
      "INSERT DATA { <a> <b> <c> }; DELETE WHERE { ?s ?p ?o }",
      ElementsAre(AllOf(insertMatcher,
                        m::pq::OriginalString("INSERT DATA { <a> <b> <c> }")),
                  AllOf(deleteWhereAllMatcher,
                        m::pq::OriginalString("DELETE WHERE { ?s ?p ?o }"))));
  expectUpdate_(
      "PREFIX foo: <foo/> INSERT DATA { <a> <b> <c> }; INSERT DATA { foo:a "
      "foo:b foo:c }",
      ElementsAre(
          AllOf(insertMatcher,
                m::pq::OriginalString(
                    "PREFIX foo: <foo/> INSERT DATA { <a> <b> <c> }")),
          AllOf(fooInsertMatcher,
                m::pq::OriginalString("INSERT DATA { foo:a foo:b foo:c }"))));
  expectUpdate_(
      "PREFIX foo: <bar/> INSERT DATA { <a> <b> <c> }; PREFIX foo: <foo/> "
      "INSERT DATA { foo:a foo:b foo:c }",
      ElementsAre(
          AllOf(insertMatcher,
                m::pq::OriginalString(
                    "PREFIX foo: <bar/> INSERT DATA { <a> <b> <c> }")),
          AllOf(fooInsertMatcher,
                m::pq::OriginalString(
                    "PREFIX foo: <foo/> INSERT DATA { foo:a foo:b foo:c }"))));
  expectUpdate_("", testing::IsEmpty());
  expectUpdate_(" ", testing::IsEmpty());
  expectUpdate_("PREFIX ex: <http://example.org>", testing::IsEmpty());
  expectUpdate_("INSERT DATA { <a> <b> <c> }; PREFIX ex: <http://example.org>",
                testing::ElementsAre(insertMatcher));
  expectUpdate_("### Some comment \n \n #someMoreComments", testing::IsEmpty());
  expectUpdate_(
      "INSERT DATA { <a> <b> <c> };### Some comment \n \n #someMoreComments",
      testing::ElementsAre(insertMatcher));
}

TEST(SparqlParser, Create) {
  auto expectCreate = ExpectCompleteParse<&Parser::create>{defaultPrefixMap};
  auto expectCreateFails = ExpectParseFails<&Parser::create>{defaultPrefixMap};

  expectCreate("CREATE GRAPH <foo>", testing::IsEmpty());
  expectCreate("CREATE SILENT GRAPH <foo>", testing::IsEmpty());
  expectCreateFails("CREATE <foo>");
  expectCreateFails("CREATE ?foo");
}

TEST(SparqlParser, Add) {
  auto expectAdd = ExpectCompleteParse<&Parser::add>{defaultPrefixMap};
  auto expectAddFails = ExpectParseFails<&Parser::add>{defaultPrefixMap};
  auto Iri = TripleComponent::Iri::fromIriref;

  auto addMatcher = ElementsAre(m::AddAll(Iri("<foo>"), Iri("<bar>")));
  expectAdd("ADD GRAPH <baz> to GRAPH <baz>", IsEmpty());
  expectAdd("ADD DEFAULT TO DEFAULT", IsEmpty());
  expectAdd("ADD GRAPH <foo> TO GRAPH <bar>", addMatcher);
  expectAdd("ADD SILENT GRAPH <foo> TO <bar>", addMatcher);
  expectAdd("ADD <foo> to DEFAULT",
            ElementsAre(m::AddAll(Iri("<foo>"), Iri(DEFAULT_GRAPH_IRI))));
  expectAdd("ADD GRAPH <foo> to GRAPH <foo>", testing::IsEmpty());
  expectAddFails("ADD ALL TO NAMED");
}

TEST(SparqlParser, Clear) {
  auto expectClear = ExpectCompleteParse<&Parser::clear>{defaultPrefixMap};
  auto expectClearFails = ExpectParseFails<&Parser::clear>{defaultPrefixMap};
  auto Iri = TripleComponent::Iri::fromIriref;

  expectClear("CLEAR ALL", m::Clear(Variable("?g")));
  expectClear("CLEAR SILENT GRAPH <foo>", m::Clear(Iri("<foo>")));
  expectClear("CLEAR NAMED", m::Clear(Variable("?g"),
                                      "?g != "
                                      "<http://qlever.cs.uni-freiburg.de/"
                                      "builtin-functions/default-graph>"));
  expectClear("CLEAR DEFAULT", m::Clear(Iri(DEFAULT_GRAPH_IRI)));
}

TEST(SparqlParser, Drop) {
  // TODO: deduplicate with clear which is the same in our case (implicit
  // graph existence)
  auto expectDrop = ExpectCompleteParse<&Parser::drop>{defaultPrefixMap};
  auto expectDropFails = ExpectParseFails<&Parser::drop>{defaultPrefixMap};
  auto Iri = TripleComponent::Iri::fromIriref;

  expectDrop("DROP ALL", m::Clear(Variable("?g")));
  expectDrop("DROP SILENT GRAPH <foo>", m::Clear(Iri("<foo>")));
  expectDrop("DROP NAMED", m::Clear(Variable("?g"),
                                    "?g != "
                                    "<http://qlever.cs.uni-freiburg.de/"
                                    "builtin-functions/default-graph>"));
  expectDrop("DROP DEFAULT", m::Clear(Iri(DEFAULT_GRAPH_IRI)));
}

TEST(SparqlParser, Move) {
  auto expectMove = ExpectCompleteParse<&Parser::move>{defaultPrefixMap};
  auto expectMoveFails = ExpectParseFails<&Parser::move>{defaultPrefixMap};
  auto Iri = TripleComponent::Iri::fromIriref;

  // Moving a graph onto itself changes nothing
  expectMove("MOVE SILENT DEFAULT TO DEFAULT", testing::IsEmpty());
  expectMove("MOVE GRAPH <foo> TO <foo>", testing::IsEmpty());
  expectMove("MOVE GRAPH <foo> TO DEFAULT",
             ElementsAre(m::Clear(Iri(DEFAULT_GRAPH_IRI)),
                         m::AddAll(Iri("<foo>"), Iri(DEFAULT_GRAPH_IRI)),
                         m::Clear(Iri("<foo>"))));
}

TEST(SparqlParser, Copy) {
  auto expectCopy = ExpectCompleteParse<&Parser::copy>{defaultPrefixMap};
  auto expectCopyFails = ExpectParseFails<&Parser::copy>{defaultPrefixMap};
  auto Iri = TripleComponent::Iri::fromIriref;

  // Copying a graph onto itself changes nothing
  expectCopy("COPY SILENT DEFAULT TO DEFAULT", testing::IsEmpty());
  expectCopy("COPY GRAPH <foo> TO <foo>", testing::IsEmpty());
  expectCopy("COPY DEFAULT TO GRAPH <foo>",
             ElementsAre(m::Clear(Iri("<foo>")),
                         m::AddAll(Iri(DEFAULT_GRAPH_IRI), Iri("<foo>"))));
}

TEST(SparqlParser, Load) {
  auto expectLoad = ExpectCompleteParse<&Parser::load>{defaultPrefixMap};
  auto Iri = [](std::string_view stringWithBrackets) {
    return TripleComponent::Iri::fromIriref(stringWithBrackets);
  };
  auto noGraph = std::monostate{};

  expectLoad(
      "LOAD <https://example.com>",
      m::UpdateClause(
          m::GraphUpdate({}, {SparqlTripleSimpleWithGraph{Var("?s"), Var("?p"),
                                                          Var("?o"), noGraph}}),
          m::GraphPattern(m::Load(Iri("<https://example.com>"), false))));
  expectLoad("LOAD SILENT <http://example.com> into GRAPH <bar>",
             m::UpdateClause(
                 m::GraphUpdate(
                     {}, {SparqlTripleSimpleWithGraph{
                             Var("?s"), Var("?p"), Var("?o"), Iri("<bar>")}}),
                 m::GraphPattern(m::Load(Iri("<http://example.com>"), true))));
}

TEST(SparqlParser, GraphOrDefault) {
  // Explicitly test this part, because all features that use it are not yet
  // supported.
  auto expectGraphOrDefault =
      ExpectCompleteParse<&Parser::graphOrDefault>{defaultPrefixMap};
  expectGraphOrDefault("DEFAULT", testing::VariantWith<DEFAULT>(testing::_));
  expectGraphOrDefault(
      "GRAPH <foo>",
      testing::VariantWith<GraphRef>(AD_PROPERTY(
          TripleComponent::Iri, toStringRepresentation, testing::Eq("<foo>"))));
}

TEST(SparqlParser, GraphRef) {
  auto expectGraphRefAll =
      ExpectCompleteParse<&Parser::graphRefAll>{defaultPrefixMap};
  expectGraphRefAll("DEFAULT", m::Variant<DEFAULT>());
  expectGraphRefAll("NAMED", m::Variant<NAMED>());
  expectGraphRefAll("ALL", m::Variant<ALL>());
  expectGraphRefAll("GRAPH <foo>", m::GraphRefIri("<foo>"));
}

TEST(SparqlParser, QuadsNotTriples) {
  auto expectQuadsNotTriples =
      ExpectCompleteParse<&Parser::quadsNotTriples>{defaultPrefixMap};
  auto expectQuadsNotTriplesFails =
      ExpectParseFails<&Parser::quadsNotTriples>{};
  const auto Iri = TripleComponent::Iri::fromIriref;
  auto GraphBlock = [](const ad_utility::sparql_types::VarOrIri& graph,
                       const ad_utility::sparql_types::Triples& triples)
      -> testing::Matcher<const Quads::GraphBlock&> {
    return testing::FieldsAre(testing::Eq(graph),
                              testing::ElementsAreArray(triples));
  };

  expectQuadsNotTriples(
      "GRAPH <foo> { <a> <b> <c> }",
      GraphBlock(Iri("<foo>"), {{::Iri("<a>"), ::Iri("<b>"), ::Iri("<c>")}}));
  expectQuadsNotTriples(
      "GRAPH ?f { <a> <b> <c> }",
      GraphBlock(Var("?f"), {{::Iri("<a>"), ::Iri("<b>"), ::Iri("<c>")}}));
  expectQuadsNotTriplesFails("GRAPH \"foo\" { <a> <b> <c> }");
  expectQuadsNotTriplesFails("GRAPH _:blankNode { <a> <b> <c> }");
}

TEST(SparqlParser, SourceSelector) {
  // This will be implemented soon, but for now we test the failure for the
  // coverage tool.
  auto expectSelector = ExpectCompleteParse<&Parser::sourceSelector>{};
  expectSelector("<x>", m::TripleComponentIri("<x>"));

  auto expectNamedGraph = ExpectCompleteParse<&Parser::namedGraphClause>{};
  expectNamedGraph("NAMED <x>", m::TripleComponentIri("<x>"));

  auto expectDefaultGraph = ExpectCompleteParse<&Parser::defaultGraphClause>{};
  expectDefaultGraph("<x>", m::TripleComponentIri("<x>"));
}

// _____________________________________________________________________________
TEST(ParserTest, propertyPathInCollection) {
  std::string query =
      "PREFIX : <http://example.org/>\n"
      "SELECT * { ?s ?p ([:p* 123] [^:r \"hello\"]) }";
  EXPECT_THAT(
      SparqlParser::parseQuery(std::move(query)),
      m::SelectQuery(
          m::AsteriskSelect(),
          m::GraphPattern(m::Triples(
              {{Var{"?_QLever_internal_variable_2"},
                "<http://www.w3.org/1999/02/22-rdf-syntax-ns#first>",
                Var{"?_QLever_internal_variable_1"}},
               {Var{"?_QLever_internal_variable_2"},
                "<http://www.w3.org/1999/02/22-rdf-syntax-ns#rest>",
                iri("<http://www.w3.org/1999/02/22-rdf-syntax-ns#nil>")},
               {Var{"?_QLever_internal_variable_1"},
                PropertyPath::makeWithChildren(
                    {PropertyPath::fromIri("<http://example.org/r>")},
                    PropertyPath::Operation::INVERSE),
                lit("\"hello\"")},
               {Var{"?_QLever_internal_variable_3"},
                "<http://www.w3.org/1999/02/22-rdf-syntax-ns#first>",
                Var{"?_QLever_internal_variable_0"}},
               {Var{"?_QLever_internal_variable_3"},
                "<http://www.w3.org/1999/02/22-rdf-syntax-ns#rest>",
                Var{"?_QLever_internal_variable_2"}},
               {Var{"?_QLever_internal_variable_0"},
                PropertyPath::makeModified(
                    PropertyPath::fromIri("<http://example.org/p>"), "*"),
                123},
               {Var{"?s"}, Var{"?p"}, Var{"?_QLever_internal_variable_3"}}}))));
}

TEST(SparqlParser, Datasets) {
  auto expectUpdate = ExpectCompleteParse<&Parser::update>{defaultPrefixMap};
  auto expectQuery = ExpectCompleteParse<&Parser::query>{defaultPrefixMap};
  auto expectAsk = ExpectCompleteParse<&Parser::askQuery>{defaultPrefixMap};
  auto expectConstruct =
      ExpectCompleteParse<&Parser::constructQuery>{defaultPrefixMap};
  auto expectDescribe =
      ExpectCompleteParse<&Parser::describeQuery>{defaultPrefixMap};
  auto Iri = [](std::string_view stringWithBrackets) {
    return TripleComponent::Iri::fromIriref(stringWithBrackets);
  };
  auto noGraph = std::monostate{};
  auto noGraphs = m::Graphs{};
  ScanSpecificationAsTripleComponent::Graphs datasets{{Iri("<g>")}};
  // Only checks `_filters` on the GraphPattern. We are not concerned with the
  // `_graphPatterns` here.
  auto filterGraphPattern = m::Filters(m::ExistsFilter(
      m::GraphPattern(m::Triples({{Var("?a"), Var{"?b"}, Var("?c")}})),
      datasets, noGraphs));
  // Check that datasets are propagated correctly into the different types of
  // operations.
  expectUpdate(
      "DELETE { ?x <b> <c> } USING <g> WHERE { ?x ?y ?z FILTER EXISTS {?a ?b "
      "?c} }",
      testing::ElementsAre(m::UpdateClause(
          m::GraphUpdate({{Var("?x"), Iri("<b>"), Iri("<c>"), noGraph}}, {}),
          filterGraphPattern, m::datasetClausesMatcher(datasets, noGraphs))));
  expectQuery("SELECT * FROM <g> WHERE { ?x ?y ?z FILTER EXISTS {?a ?b ?c} }",
              m::SelectQuery(m::AsteriskSelect(), filterGraphPattern, datasets,
                             noGraphs));
  expectAsk("ASK FROM <g> { ?x ?y ?z FILTER EXISTS {?a ?b ?c}}",
            m::AskQuery(filterGraphPattern, datasets, noGraphs));
  expectConstruct(
      "CONSTRUCT {<a> <b> <c>} FROM <g> { ?x ?y ?z FILTER EXISTS {?a ?b?c}}",
      m::ConstructQuery(
          {std::array<GraphTerm, 3>{::Iri("<a>"), ::Iri("<b>"), ::Iri("<c>")}},
          filterGraphPattern, datasets, noGraphs));
  // See comment in visit function for `DescribeQueryContext`.
  expectDescribe(
      "Describe ?x FROM <g> { ?x ?y ?z FILTER EXISTS {?a ?b ?c}}",
      m::DescribeQuery(
          m::Describe({Var("?x")}, {datasets, {}},
                      m::SelectQuery(m::VariablesSelect({"?x"}, false, false),
                                     filterGraphPattern)),
          datasets, noGraphs));
}
