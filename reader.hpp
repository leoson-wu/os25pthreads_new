#include <fstream>
#include "thread.hpp"
#include "ts_queue.hpp"
#include "item.hpp"

#ifndef READER_HPP
#define READER_HPP

class Reader : public Thread {
public:
	// constructor
	Reader(int expected_lines, std::string input_file, TSQueue<Item*>* input_queue);

	// destructor
	~Reader();

	virtual void start() override;
private:
	// the expected lines to read,
	// the reader thread finished after input expected lines of item
	int expected_lines;

	std::ifstream ifs;
	TSQueue<Item*>* input_queue;

	// the method for pthread to create a reader thread
	static void* process(void* arg);
};

// Implementaion start

Reader::Reader(int expected_lines, std::string input_file, TSQueue<Item*>* input_queue)
	: expected_lines(expected_lines), input_queue(input_queue) {
	ifs = std::ifstream(input_file);
}

Reader::~Reader() {
	ifs.close();
}

void Reader::start() {
	pthread_create(&t, 0, Reader::process, (void*)this);
	/*
	arg1: pointer to pthread_t -> thread 創建成功後會獲得該 thread 的 ID
	arg2: pthread attributes, 0 means default attributes
	arg3: pointer to the function to be executed by the thread
	arg4: argument to be passed to the function
	*/
}

void* Reader::process(void* arg) {
	Reader* reader = (Reader*)arg;

	while (reader->expected_lines--) {
		Item *item = new Item;
		reader->ifs >> *item;
		reader->input_queue->enqueue(item);
	}

	return nullptr;
}

#endif // READER_HPP
