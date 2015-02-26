#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/lexical_cast.hpp>

#include <array>
#include <iostream>
#include <thread>
#include <vector>
#include <sstream>
#include <algorithm>

using namespace std;
using namespace boost;
using namespace asio;
using namespace ip;

int main(int argc, char *argv[]){
	io_service io_service;
	io_service::work work(io_service);

	thread t([&]{
		io_service.run();
	});

	tcp::resolver resolver(io_service);
	auto iter = resolver.resolve({"foo.kassala.de", "1234"});
	tcp::socket socket(io_service, tcp::v4());

	auto connection = async_connect(socket, iter, use_future);
	connection.get();
	cerr << "yay :D\n";

	const string data = R"(18{"get":"networks"})";
	auto wrote = async_write(socket, buffer(data), use_future);

	vector<char> read_data(40);
	const auto print_read_data = [&]{
		for(auto && c : read_data) {
			if(!c) break;
			cerr << c;
		}
		cerr << endl;
	};

	auto read = async_read(socket, buffer(read_data), transfer_at_least(3), use_future);
	const auto read_bytes = read.get();

	cerr << "wrote " << wrote.get() << endl;
	cerr << "read " << read_bytes << endl;

	string s(read_data.begin(), find_if_not(read_data.begin(), read_data.end(), &::isdigit));
	const auto total_bytes = lexical_cast<size_t>(s);
	print_read_data();

	const auto read_message_bytes = read_bytes - s.size();
	if(total_bytes > read_message_bytes){
		const auto remaining = total_bytes - read_message_bytes;
		read_data.assign(remaining, 0);
		auto read = async_read(socket, buffer(read_data), use_future);
		read.get();

		print_read_data();
	}

	io_service.stop();
	t.join();
}

