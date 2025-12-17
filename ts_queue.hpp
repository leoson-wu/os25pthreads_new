#include <pthread.h>

#ifndef TS_QUEUE_HPP
#define TS_QUEUE_HPP

#define DEFAULT_BUFFER_SIZE 200

template <class T>
class TSQueue {
public:
	// constructor
	TSQueue();

	explicit TSQueue(int max_buffer_size);

	// destructor
	~TSQueue();

	// add an element to the end of the queue
	void enqueue(T item);

	// remove and return the first element of the queue
	T dequeue();

	// return the number of elements in the queue
	int get_size();
private:
	// the maximum buffer size
	int buffer_size;
	// the buffer containing values of the queue
	T* buffer;
	// the current size of the buffer
	int size;
	// the index of first item in the queue
	int head;
	// the index of last item in the queue
	int tail;

	// pthread mutex lock
	pthread_mutex_t mutex;
	// pthread conditional variable
	pthread_cond_t cond_enqueue, cond_dequeue;
};

// Implementation start

template <class T>
TSQueue<T>::TSQueue() : TSQueue(DEFAULT_BUFFER_SIZE) {
}

template <class T>
TSQueue<T>::TSQueue(int buffer_size) : buffer_size(buffer_size) {
	// TODO: implements TSQueue constructor

	// 1. 在記憶體中配置 buffer 的空間
	buffer  = new T[buffer_size];
	// 2. initialize tsqueue 的初始狀態
	size = 0;
	head = 0;
	tail = 0;
	// 3. 初始化 mutex 以及 condition variables
	pthread_mutex_init(&mutex, nullptr);
	pthread_cond_init(&cond_enqueue, nullptr); // 讓 producer waiting 的條件變數
	pthread_cond_init(&cond_dequeue, nullptr); // 讓 consumer waiting 的條件變數
}

template <class T>
TSQueue<T>::~TSQueue() {
	// TODO: implenents TSQueue destructor

	// 1. 釋放 buffer 的記憶體
	delete[] buffer;
	// 2. destory mutex鎖 以及 condition variables
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&cond_enqueue);
	pthread_cond_destroy(&cond_dequeue);
}

template <class T>
void TSQueue<T>::enqueue(T item) {
	// TODO: enqueues an element to the end of the queue

	pthread_mutex_lock(&mutex); // lock mutex
	// 1. 當 buffer is full, 用 busy waitting 卡 producer 持續等待直到 consumer dequeue
	while (size == buffer_size) {
		pthread_cond_wait(&cond_enqueue, &mutex);
	}
	// 2. 將 item 放入 buffer 的 tail
	buffer[tail] = item;
	tail = (tail + 1) % buffer_size; // update 尾端的 index
	size++; // update 新增一個 item 後的 size
	pthread_cond_signal(&cond_dequeue); // 嘗試 wake up consumer (因為 buffer 中有資料了, comsumer 若卡在 waiting condition 下可以被 waken up)
	pthread_mutex_unlock(&mutex); // unlock mutex
}

template <class T>
T TSQueue<T>::dequeue() {
	// TODO: dequeues the first element of the queue

	T item;
	pthread_mutex_lock(&mutex);

	// 1. 當 buffer is empty, 用 busy waitting 持續等待直到 producer enqueue
	while (size == 0) {
		pthread_cond_wait(&cond_dequeue, &mutex);
	}
	// 2. 從 buffer 的 head 拿出 item
	item = buffer[head];
	head = (head + 1) % buffer_size; // update 頭端的 index
	size--; // update 減少一個 item 後的 size
	pthread_cond_signal(&cond_enqueue); // 嘗試 wake up producer (因為 buffer 中有空間了, producer 若卡在 waiting condition 下可以被 waken up)
	pthread_mutex_unlock(&mutex);

	return item;
}

template <class T>
int TSQueue<T>::get_size() {
	// TODO: returns the size of the queue
	int current_size;
	pthread_mutex_lock(&mutex);
	current_size = size; 
	pthread_mutex_unlock(&mutex);
	return current_size;
}

#endif // TS_QUEUE_HPP
