#include <boost/test/unit_test.hpp>

#include <util/util.hpp>

BOOST_AUTO_TEST_SUITE(util)

BOOST_AUTO_TEST_CASE(url_decode, *boost::unit_test::timeout(180))
{
    std::string in1 = "foo%40bar";
    std::string in2 = "foo%2540bar";
    std::string in3 = "foo%40";
    std::string in4 = "%40bar";
    std::string in5 = "foo%22bar";
    std::string in6 = "foo_bar";
    std::string in7 = "foo~bar";

    std::string out = nabto::urlDecode(in1);
    BOOST_TEST(out == "foo@bar");

    out = nabto::urlDecode(in2);
    BOOST_TEST(out == "foo%40bar");

    out = nabto::urlDecode(in3);
    BOOST_TEST(out == "foo@");

    out = nabto::urlDecode(in4);
    BOOST_TEST(out == "@bar");

    out = nabto::urlDecode(in5);
    BOOST_TEST(out == "foo\"bar");

    out = nabto::urlDecode(in6);
    BOOST_TEST(out == in6);

    out = nabto::urlDecode(in7);
    BOOST_TEST(out == in7);
}

BOOST_AUTO_TEST_SUITE_END()
