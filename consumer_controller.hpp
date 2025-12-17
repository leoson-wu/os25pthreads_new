#include <pthread.h>
#include <unistd.h>
#include <vector>
#include <iostream>
#include "consumer.hpp"
#include "ts_queue.hpp"
#include "item.hpp"
#include "transformer.hpp"

#ifndef CONSUMER_CONTROLLER
#define CONSUMER_CONTROLLER

class ConsumerController : public Thread {
public:
	// constructor
	ConsumerController(
		TSQueue<Item*>* worker_queue,
		TSQueue<Item*>* writer_queue,
		Transformer* transformer,
		int check_period,
		int low_threshold,
		int high_threshold
	);

	// destructor
	~ConsumerController();

	virtual void start();

private:
	std::vector<Consumer*> consumers;

	TSQueue<Item*>* worker_queue;
	TSQueue<Item*>* writer_queue;

	Transformer* transformer;

	// Check to scale down or scale up every check period in microseconds.
	int check_period;
	// When the number of items in the worker queue is lower than low_threshold,
	// the number of consumers scaled down by 1.
	int low_threshold;
	// When the number of items in the worker queue is higher than high_threshold,
	// the number of consumers scaled up by 1.
	int high_threshold;

	static void* process(void* arg);
};

// Implementation start

ConsumerController::ConsumerController(
	TSQueue<Item*>* worker_queue,
	TSQueue<Item*>* writer_queue,
	Transformer* transformer,
	int check_period,
	int low_threshold,
	int high_threshold
) : worker_queue(worker_queue),
	writer_queue(writer_queue),
	transformer(transformer),
	check_period(check_period),
	low_threshold(low_threshold),
	high_threshold(high_threshold) {
}

ConsumerController::~ConsumerController() {
    // 先對所有 Consumer 發出 Cancel
    for (Consumer* consumer : consumers) {
        consumer->cancel();
    }
    // 等待它們全部結束並釋放記憶體
    for (Consumer* consumer : consumers) {
        consumer->join();
        delete consumer;
    }
    consumers.clear();
}


void ConsumerController::start() {
	// TODO: starts a ConsumerController thread
	pthread_create(&t, 0, ConsumerController::process, (void*)this);
}

void* ConsumerController::process(void* arg) {
	// TODO: implements the ConsumerController's work
	ConsumerController* controller = (ConsumerController*)arg;

	// 透過 usleep 等待 check_period 後定期檢查 worker_queue 的 size
	while (true) {
		// 1. 等待 check_period 微秒
		usleep(controller->check_period);
		// 2. 取得 worker_queue 的 size
		int current_size = controller->worker_queue->get_size();
		// 3. 根據 current_size 決定scale up or scale down consumer

		if (current_size > controller->high_threshold) { // Scale up

			// scale up 前 consumers 的數量 
			int old_count = controller->consumers.size();
			// create 新的一個 comsumer
			Consumer* new_consumer = new Consumer(
				controller->worker_queue,
				controller->writer_queue,
				controller->transformer
			);
			// start 這個新的 consumer thread
			new_consumer->start();
			// 新增這個 consumer 到 controller 的 consumers list 中
			controller->consumers.push_back(new_consumer);

			// scale up 後 consumers 的數量 (理論上應該為 old_count + 1)
			int new_count = controller->consumers.size();
			std::cout << "Scaling up consumers from " << old_count << " to " << new_count << std::endl;

		}else if (current_size < controller->low_threshold && controller->consumers.size() > 1) { // Scale down
			int old_count = controller->consumers.size();

			// 0. 取得最後一個 consumer
			Consumer* target = controller->consumers.back();

			// 1. cancel 這個 consumer thread
			target->cancel();
			// revision note:
			// 2. 等待 thread 結束
			target->join();
			// 3. 釋放 consumer 配置的記憶體
			delete target;

			// 從 controller 的 consumers list 中移除這個 consumer
			controller->consumers.pop_back();

			// scale down 後 consumers 的數量(理論上應該為 old_count - 1)
			int new_count = controller->consumers.size();
			std::cout << "Scaling down consumers from " << old_count << " to " << new_count << std::endl;
		
		}
	}
	return nullptr;
}
#endif // CONSUMER_CONTROLLER_HPP
