#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/numeric/conversion/cast.hpp>

#include <array>
#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <list>
#include <sstream>
#include <algorithm>

#define BARK clog << __LINE__ << ": "
#define BARKN BARK << endl;

using namespace std;
using namespace boost;
using namespace asio;
using namespace ip;

struct rawdapi {
	rawdapi(io_service& servi, tcp::socket& socki)
	: service(servi)
	, socket(socki)
	{}

	io_service& service;
	tcp::socket& socket;

	queue<string> incoming;
	struct outgoing_message {
		string data;
		list<promise<string>>::iterator promise_it;
	};

	queue<outgoing_message> outgoing;
	list<promise<string>> responses;

	auto write(string x){
		auto const writing = !outgoing.empty();
		auto it = responses.insert(responses.end(), promise<string>{});
		outgoing_message m{x, it};
		outgoing.push(m);

		if(!writing){
			do_write();
		}

		return it->get_future();
	}

	void do_write(){
		do_write(outgoing.front());
	}

	void do_write(outgoing_message& msg){
		auto& x = msg.data;
		x = lexical_cast<string>(x.size()) + x;

		async_write(socket, buffer(x), [this, it = msg.promise_it](auto ec, auto /*length*/){
			if(!ec){
				outgoing.pop();
				if(!outgoing.empty()){
					do_write();
				}
			} else {
				it->set_exception(make_exception_ptr(runtime_error("Failed to write")));
				socket.close();
			}
		});
	}

	vector<char> current_incoming;

	void abandon_ship(){
		for(auto& promis : responses){
			promis.set_exception(make_exception_ptr(runtime_error("Something failed, abandoning ship")));
		}
		socket.close();
	}

	void do_read(){
		current_incoming.assign(1024, 0);
		async_read(socket, buffer(current_incoming), transfer_at_least(4), [this](auto ec, auto read_bytes){
			if(ec){
				abandon_ship();
				return;
			}

			auto const end_of_bytes = current_incoming.begin() + numeric_cast<int>(read_bytes);

			auto const end_of_size = find_if_not(current_incoming.begin(), end_of_bytes, &::isdigit);
			string const size_string(current_incoming.begin(), end_of_size);
			const auto total_bytes = lexical_cast<size_t>(size_string);

			string answer(end_of_size, end_of_bytes);

			const auto read_message_bytes = read_bytes - size_string.size();
			auto const theres_more = total_bytes > read_message_bytes;
			if(theres_more){
				const auto remaining = total_bytes - read_message_bytes + 1;
				current_incoming.assign(remaining, 0);
				read_bytes = read(socket, buffer(current_incoming));
				auto next_level_answer = string(current_incoming.begin(), current_incoming.begin() + numeric_cast<int>(read_bytes));
				answer += next_level_answer;
			}

			if(answer.substr(2, 5) == "event"){
				incoming.push(answer);
			} else {
				if(responses.empty()){
					throw std::runtime_error("No promises to fulfill");
				}

				auto current_promise = move(responses.front());
				responses.erase(responses.begin());

				current_promise.set_value(answer);
			}

			do_read();
		});
	}

	bool dummy_bootstrap = [this]{ do_read(); return !true; }();
};

int main(){
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
	BARK << "yay :D\n";

	rawdapi api{io_service, socket};

	auto networks = api.write(R"({"get":"networks"})");
	auto kassala_channels = api.write(R"({"get":"channels", "params":["kassala"]})");
	auto nick_network = api.write(R"({"get":"nick", "params":["kassala"]})");

	cout << "Nick: " << nick_network.get() << endl;
	cout << "Channels: " << kassala_channels.get() << endl;
	cout << "Networks: " << networks.get() << endl;

	t.join();
}

