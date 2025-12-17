# Pthreads Implementation Report

## 組員貢獻度表格

| 貢獻項目 | 名字 | 系所 |
|---------|------|------|
| Implementation | 吳征彥、許恩嘉 | 資工碩二、資應碩二 |
| Experiment |  吳征彥、許恩嘉 | 資工碩二、資應碩二 |
| Report |  吳征彥、許恩嘉 | 資工碩二、資應碩二 |

## Code Structure

### TSQueue
在`ts_queue.hpp`中利用 pthread lib 實作 threads safe queue，作為 buffer 空間的資料結構，  
目的是可以支援多個 threads cocurrently 的 **共享**/**存取** 這個空間  
1. constructor:  
   - 配置 **buffer** 空間，並以 **queue** 的結構管理，支援 `enqueue()`, `dequeue()`, `get_size()` 的 member functions  
   - 為了避免 **race condition** 以及 **維持 threads 之間對於 buffer 的同步化** 需要 `pthread lib` 的以下物件:  
     - `mutex lock:` ts_queue 自己的鎖
     - `condition variable:` 通知/控制 **producer threads** 以及 **consumer threads** 等待  
   ```cpp
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
   ```  
2. destructor:  
   安全釋放 buffer 的記憶體空間以及同步化的資源(mutex lock, cond_enqueue, cond_dequeue)  
   ```cpp
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
    ```
3. enqueue
   ```cpp
    template <class T>
    void TSQueue<T>::enqueue(T item) {
        // TODO: enqueues an element to the end of the queue

        pthread_mutex_lock(&mutex);
        // 1. 當 buffer is full, 用 busy waitting 卡 producer 持續等待直到 consumer dequeue
        while (size == buffer_size) {
            pthread_cond_wait(&cond_enqueue, &mutex);
        }
        // 2. 將 item 放入 buffer 的 tail
        buffer[tail] = item;
        tail = (tail + 1) % buffer_size; // update 尾端的 index
        size++; // update 新增一個 item 後的 size
        pthread_cond_signal(&cond_dequeue); // 嘗試 wake up consumer (因為 buffer 中有資料了, comsumer 若卡在 waiting condition 下可以被 waken up)
        pthread_mutex_unlock(&mutex);
    }
    ```
4. dequeue
   ```cpp
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
   ```
5. get_size
   ```cpp
    template <class T>
    int TSQueue<T>::get_size() {
        // TODO: returns the size of the queue
        int current_size;
        pthread_mutex_lock(&mutex);
        current_size = size;
        pthread_mutex_unlock(&mutex);
        return current_size;
    }
   ```
   - **問題探討:**  
   在 get_size() 的實作中為什麼要加上 `mutex lock` 來保護 `size` 這個變數?  
   他明明只是**一條讀記憶體的指令(MOV)**，理論上不會產生 race condition，跟課堂中的例子(counter++)的情況不一樣。  
   - **解釋:**
   在查詢相關資料後，再多核且多執行緒的環境下，每個 CPU 有自己的 local cache，如果一個thread 在自己的 local cache 改變了 size 的值但並未更新到 main memory 其他 thread (e.g., comsumerController) 有可能讀到舊的值，而pthread 的 mutex lock 工具很強大，可以用來**同步記憶體**，所以在這邊的作用不是保證互斥，而是做記憶體階層的同步。   


### Reader
在`reader.hpp`中助教已實作 reader thread。  
1. start (助教已實作)  
   
   ```cpp
    void Reader::start() {
        // 其中第三個 arg 是這類 thread 要執行的函式: process，助教已實作
        pthread_create(&t, 0, Reader::process, (void*)this);
    }
   ```
2. process (助教已實作, skip)
### Producer
1. start
   ```cpp
    void Producer::start() {
        // TODO: starts a Producer thread
        pthread_create(&t, 0, Producer::process, (void*)this);
    }
   ```
2. process
   
   ```cpp
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
                break;
            }
            // 2. do the producing transformation
            item->val = producer->transformer->producer_transform(item->opcode, item->val);
            // 3. 將 transform 後的 item 放入 worker_queue
            producer->worker_queue->enqueue(item);
        }
        return nullptr;
    }
   ```
   - **問題探討:**  
   在迴圈中為什麼要用無窮 while(true) 迴圈的寫法，從 input_queue 中不斷取 item，直到取到 nullptr?  
   為何不是使用 `while(input_queue->get_size() > 0)` 先做檢查後才去取得item?  
   - **解釋:**  
   在 multi-thread programming 的環境下，會發生所謂的 **check-then-act** 的情況，當一個thread check queue 有東西後，準備拿東西之前，就已經被contex switch，其他thread在原本的thread 還沒取 item 時，另一條 thread 可能優先把最後一個 item 取走(即 contex switch 發生在 check 與 act 之間)，**race conditoin** 就發生了。  
   為了避免上述情況，我們可以改為先取 item 之後再來判斷內容物狀態(是不是 nullptr)，即可化解上述 race condition。

### Writer
1. start
   ```cpp
    void Writer::start() {
        // TODO: starts a Writer thread
        // 第三個 arg 為此 thread 會去執行的 function
        pthread_create(&t, 0, Writer::process, (void*)this); 
    }
   ```
2. process
   ```cpp
    void* Writer::process(void* arg) {
        // TODO: implements the Writer's work

        Writer* writer = (Writer*)arg;

        // write expected_lines 行 item 到 output file
        for (int i = 0; i < writer->expected_lines; i++) {
            // 1. 從 output_queue 中取出 item
            Item* item = writer->output_queue->dequeue();

            // 2. 將 item 寫入 output file
            
            writer->ofs << *item;
            delete item; // 寫入後釋放 item 記憶體
        }
        return nullptr;
    }
   ```


### Consumer
1. start
   ```cpp
    void Consumer::start() {
        // TODO: starts a Consumer thread
        pthread_create(&t, 0, Consumer::process, (void*)this);
    }
   ```
2. cancel
   ```cpp
    int Consumer::cancel() {
        // TODO: cancels the consumer thread
        is_cancel = true; // 宣告一個 cancel flag 作為取消標記
        return pthread_cancel(t);
    }
   ```
3. process
   ```cpp
    void* Consumer::process(void* arg) {
        Consumer* consumer = (Consumer*)arg;

        // 設置 thread 的 cancel type 為 deferred cancelation:
        // ->不馬上取消 thread, 而是在 thread 執行到可以被取消的點時才取消
        pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, nullptr);

        while (!consumer->is_cancel) {
            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr);

            // TODO: implements the Consumer's work
            // consumer 負責從 worker_queue 中取出 item, 並丟到 output_queue 中
            Item* item = consumer->worker_queue->dequeue();
            
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);

            if (item == nullptr) {
                consumer->output_queue->enqueue(nullptr);
                break;
            }

            // 2. do the consuming transformation
            item->val = consumer->transformer->consumer_transform(item->opcode, item->val);
            // 3. 將 transform 後的 item 放入 output_queue
            consumer->output_queue->enqueue(item);

            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);
            
            pthread_testcancel();
        }
        // 若要被 cancel 的話, 釋放 consumer 的記憶體
        if (consumer->is_cancel) {
            delete consumer;
        }

        return nullptr;
    }
   ```

### ConsumerController
1. start
   ```cpp
    void ConsumerController::start() {
        // TODO: starts a ConsumerController thread
        pthread_create(&t, 0, ConsumerController::process, (void*)this);
    }
   ```
2. process  
   ```cpp
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
                // 取得最後一個 consumer
                Consumer* target = controller->consumers.back();
                // cancel 這個 consumer thread, 在 consumer:process 結束時會自己釋放記憶體
                target->cancel();
                // 從 controller 的 consumers list 中移除這個 consumer
                controller->consumers.pop_back();

                // scale down 後 consumers 的數量(理論上應該為 old_count - 1)
                int new_count = controller->consumers.size();
                std::cout << "Scaling down consumers from " << old_count << " to " << new_count << std::endl;
            
            }
        }
        return nullptr;
    }
   ```

### main
```cpp
    int main(int argc, char** argv) {
        assert(argc == 4);

        int n = atoi(argv[1]);
        std::string input_file_name(argv[2]);
        std::string output_file_name(argv[3]);

        // TODO: implements main function

        // 1. initialize tsqueues input_queue, worker_queue, output_queue 各一個
        TSQueue<Item*>* input_queue = new TSQueue<Item*>(READER_QUEUE_SIZE);
        TSQueue<Item*>* worker_queue = new TSQueue<Item*>(WORKER_QUEUE_SIZE);
        TSQueue<Item*>* output_queue = new TSQueue<Item*>(WRITER_QUEUE_SIZE);

        // 2. initialize transformer
        Transformer* transformer = new Transformer();

        // 3. initialize each thread: 1 reader, 1 writer, 4 producers 
        Reader* reader = new Reader(n, input_file_name, input_queue);
        Writer* writer = new Writer(n, output_file_name, output_queue);
        std::vector<Producer*> producers;
        for (int i = 0; i < 4; i++) {
            producers.push_back(new Producer(input_queue, worker_queue, transformer));
        }

        // 4.initialize 1 consumer_controller thread
        int low_threshold = WORKER_QUEUE_SIZE * CONSUMER_CONTROLLER_LOW_THRESHOLD_PERCENTAGE / 100;
        int high_threshold = WORKER_QUEUE_SIZE * CONSUMER_CONTROLLER_HIGH_THRESHOLD_PERCENTAGE / 100;

        ConsumerController* consumer_controller = new ConsumerController(
            worker_queue,
            output_queue,
            transformer,
            CONSUMER_CONTROLLER_CHECK_PERIOD,
            low_threshold,
            high_threshold
        );

        // 5. start all threads
        reader->start();
        writer->start();
        for (auto& p : producers) {
            p->start();
        }
        consumer_controller->start();

        // main 必須要等待 reader 讀完整個 input file 後才能結束
        reader->join();
        for (int i = 0; i < 4; i++) {
            input_queue->enqueue(nullptr);
        }
        for (auto& p : producers) {
            p->join();
        }
        // main 等待 writer 寫完所有內容到 ouput file 後才能結束
        writer->join();

        consumer_controller->cancel();
        // 6. 釋放 main 中配置的記憶體
        delete reader;
        delete writer;
        for (auto& producer : producers) {
            delete producer;
        }
        delete consumer_controller;
        delete transformer;
        delete input_queue;
        delete worker_queue;
        delete output_queue;

        return 0;
    }
```


