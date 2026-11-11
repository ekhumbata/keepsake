// Saved Wi-Fi networks (#43): the list, the order to try them in, and the stored form.
#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "wifi_networks/wifi_networks.h"

using wifi_networks::Network;
using wifi_networks::NetworkList;
using wifi_networks::SeenNetwork;

namespace {

std::vector<std::string> Ssids(const std::vector<Network>& networks)
{
    std::vector<std::string> out;
    for (const Network& n : networks) {
        out.push_back(n.ssid);
    }
    return out;
}

}  // namespace

TEST_CASE("networks are kept newest first, one per name, at most five")
{
    NetworkList list;
    CHECK(list.empty());
    CHECK(list.Add("Home", "home password"));
    CHECK(list.Add("Hotspot", "phone password"));
    CHECK(Ssids(list.networks()) == std::vector<std::string>{"Hotspot", "Home"});

    // Same name again: new password, and it's the newest now.
    CHECK(list.Add("Home", "changed password"));
    CHECK(Ssids(list.networks()) == std::vector<std::string>{"Home", "Hotspot"});
    CHECK(list.Find("Home")->password == "changed password");

    CHECK(list.Add("A", ""));
    CHECK(list.Add("B", "12345678"));
    CHECK(list.Add("C", "12345678"));
    CHECK(list.networks().size() == 5);
    CHECK(list.Add("D", "12345678"));  // full: the least recently used one (Hotspot) goes
    CHECK(list.networks().size() == 5);
    CHECK(list.Find("Hotspot") == nullptr);

    CHECK_FALSE(list.Add("", "x"));
    CHECK_FALSE(list.Add(std::string(33, 's'), "x"));
    CHECK(list.Remove("A"));
    CHECK_FALSE(list.Remove("A"));
    CHECK(list.networks().size() == 4);
}

TEST_CASE("a successful connection moves a network to the front")
{
    NetworkList list;
    list.Add("Home", "");
    list.Add("Work", "");
    list.Add("Cafe", "");
    list.MarkUsed("Home");
    CHECK(Ssids(list.Candidates()) == std::vector<std::string>{"Home", "Cafe", "Work"});
    list.MarkUsed("Nowhere");  // unknown: ignored
    CHECK(list.networks().size() == 3);
}

TEST_CASE("with a scan, the strongest saved network in range goes first")
{
    NetworkList list;
    list.Add("Home", "");
    list.Add("Hotspot", "");
    list.Add("Gran's", "");
    const std::vector<SeenNetwork> scan = {
        {"Neighbour", -40}, {"Home", -70}, {"Hotspot", -55}, {"Home", -60},
    };
    CHECK(Ssids(list.Candidates(&scan)) == std::vector<std::string>{"Hotspot", "Home"});

    const std::vector<SeenNetwork> none_known = {{"Neighbour", -40}};
    CHECK(Ssids(list.Candidates(&none_known)) ==
          std::vector<std::string>{"Gran's", "Hotspot", "Home"});
    const std::vector<SeenNetwork> empty_scan;
    CHECK(list.Candidates(&empty_scan).size() == 3);
}

TEST_CASE("the stored form round-trips and survives damage")
{
    NetworkList list;
    list.Add("Home", "pass\twith\ttabs");
    list.Add("Back\\slash", "");
    list.MarkUsed("Home");
    const NetworkList back = NetworkList::Parse(list.Serialize());
    CHECK(back == list);

    const NetworkList messy = NetworkList::Parse(
        "keepsake-wifi 1\r\n"
        "7\tGood\tpassword1\n"
        "x\tBadStamp\tpw\n"
        "3\t\tno name\n"
        "9\tGood\tduplicate\n"
        "99999999999\tHuge\tstamp\n"
        "2\tToo\tmany\ttabs\n"
        "5\tAlso good\t\n");
    CHECK(Ssids(messy.networks()) == std::vector<std::string>{"Good", "Also good"});
    CHECK(messy.Find("Good")->password == "password1");

    CHECK(NetworkList::Parse("keepsake-wifi 2\n1\ta\tb\n").empty());
    CHECK(NetworkList::Parse("").empty());
    std::string six = "keepsake-wifi 1\n";
    for (int i = 0; i < 6; ++i) {
        six += std::to_string(i) + "\tnet" + std::to_string(i) + "\t\n";
    }
    CHECK(NetworkList::Parse(six).networks().size() == wifi_networks::kMaxNetworks);
}

TEST_CASE("the recency counter never wraps around")
{
    NetworkList list = NetworkList::Parse("keepsake-wifi 1\n4294967295\tNewest\t\n7\tOlder\t\n");
    REQUIRE(list.networks().size() == 2);
    CHECK(list.Add("Brand new", ""));
    CHECK(Ssids(list.Candidates()) == std::vector<std::string>{"Brand new", "Newest", "Older"});
    CHECK(NetworkList::Parse(list.Serialize()) == list);
}
