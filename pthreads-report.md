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
目的是可以支援多個 threads cocurrently 的**共享**/**存取** 這個空間  
1. constructor
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
2. destructor
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
   **問題探討:**  
   在 get_size() 的實作中為什麼要加上 `mutex lock` 來保護 `size` 這個變數?  
   他明明只是**一條讀記憶體的指令(MOV)**，理論上不會產生 race condition，跟課堂中的例子(counter++)的情況不一樣。  
   **解釋:**
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
   **問題探討:**  
   在迴圈中為什麼要用無窮 while(true) 迴圈的寫法，從 input_queue 中不斷取 item，直到取到 nullptr?  
   為何不是使用 `while(input_queue->get_size() > 0)` 先做檢查後才去取得item?  
   **解釋:**  
   在 multi-thread programming 的環境下，會發生所謂的 **check-then-act** 的情況，當一個thread check queue 有東西後，準備拿東西之前，就已經被contex switch，其他thread在原本的thread 還沒取 item 時，另一條 thread 可能優先把最後一個 item 取走(即 contex switch 發生在 check 與 act 之間)，**race conditoin** 就發生了。  
   為了避免上述情況，我們可以改為先取 item 之後再來判斷內容物狀態(是不是 nullptr)，即可化解上述 race condition。

### Writer
1. start
2. 
### Transformer

### Consumer

### ConsumerController



