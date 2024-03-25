#include <boost/test/unit_test.hpp>


#include <iostream>



BOOST_AUTO_TEST_SUITE(gstreamer)

BOOST_AUTO_TEST_CASE(basic, *boost::unit_test::timeout(180))
{
    BOOST_TEST(true);
}

BOOST_AUTO_TEST_SUITE_END()
