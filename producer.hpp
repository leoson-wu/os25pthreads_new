#include <pthread.h>
#include "thread.hpp"
#include "ts_queue.hpp"
#include "item.hpp"
#include "transformer.hpp"

#ifndef PRODUCER_HPP
#define PRODUCER_HPP

class Producer : public Thread {
public:
	// constructor
	Producer(TSQueue<Item*>* input_queue, TSQueue<Item*>* worker_queue, Transformer* transfomrer);

	// destructor
	~Producer();

	virtual void start();
private:
	TSQueue<Item*>* input_queue;
	TSQueue<Item*>* worker_queue;

	Transformer* transformer;

	// the method for pthread to create a producer thread
	static void* process(void* arg);
};

Producer::Producer(TSQueue<Item*>* input_queue, TSQueue<Item*>* worker_queue, Transformer* transformer)
	: input_queue(input_queue), worker_queue(worker_queue), transformer(transformer) {
}

Producer::~Producer() {}

void Producer::start() {
	// TODO: starts a Producer thread
	pthread_create(&t, 0, Producer::process, (void*)this);
}



void* Producer::process(void* arg) {
	// TODO: implements the Producer's work
	// producer 負責從 input_queue 中取出 item, 並丟到 worker_queue 中
	Producer* producer = (Producer*)arg;

	// 不斷從 input_queue 中拿 item 出來處理
	while(true) {
		// 1. 從 input_queue 取出 item
		Item* item = producer->input_queue->dequeue();

		if (item == nullptr) {
			// 若取出的 item 是 nullptr, 表示沒有更多 item 可以處理, 結束 producer thread
			// 將 nullptr 放回 worker_queue 以通知 consumer thread 結束
			producer->worker_queue->enqueue(nullptr);
			break;
		}
		// 2. do the producing transformation
		item->val = producer->transformer->producer_transform(item->opcode, item->val);
		// 3. 將 transform 後的 item 放入 worker_queue
		producer->worker_queue->enqueue(item);
	}
	return nullptr;
}

#endif // PRODUCER_HPP
