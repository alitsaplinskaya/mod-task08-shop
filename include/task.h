#pragma once
#include <vector>
#include <string>
#include <queue>
#include <mutex>
#include <chrono>
#include <cmath>
#include <thread>
#include <iostream>
#include <atomic>

struct customer{
    std::size_t num_of_products;
    clock_t begin_time;
    std::size_t id;
    customer() = default;
    customer(std::size_t _num_of_products, clock_t _begin_time, std::size_t _id) :
    num_of_products(_num_of_products), begin_time(_begin_time), id(_id) {};
};

class my_queue{
private:
    std::queue<customer> internal_queue;
    std::mutex mutex;
    std::size_t max_queue_size;
public:
    my_queue(std::size_t _max_queue_size): max_queue_size(_max_queue_size) {};

    bool try_put(customer& cust){
        std::lock_guard<std::mutex> lock(mutex);
        if(internal_queue.size() < max_queue_size){
            internal_queue.push(cust);
            return true;
        }
        return false;
    }

    customer* take_customer(){
        customer* cust = nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if(internal_queue.size() != 0){
                cust = new customer();
                *cust = internal_queue.front();
                internal_queue.pop();
            }
        }
        return cust;
    }

    std::size_t get_size(){
        return internal_queue.size();
    }
};

class cashbox_pool{
private:
    std::vector<std::thread> cachboxes;
    my_queue customer_queue;
    std::size_t time_for_one_product;
    std::size_t max_queue_size;

public:
    //statistics
    std::vector<std::size_t> cachboxes_work_time;
    std::size_t stat_max_queue_size = 0;
    std::atomic<std::size_t> median_queue_size;
    std::atomic<std::size_t> median_customer_time{0};
    std::atomic<int> served_customers{0};


    std::atomic<bool> stop_flag{false};

    cashbox_pool(std::size_t number_of_cashboxes, std::size_t _time_for_one_product, std::size_t max_queue_size) : customer_queue(max_queue_size), time_for_one_product(_time_for_one_product) {
        cachboxes_work_time.resize(number_of_cashboxes);
        for(std::size_t i = 0; i < number_of_cashboxes; ++i){
            cachboxes_work_time[i] = 0;
            cachboxes.emplace_back([&, i](){
                while(!stop_flag.load()){
                    customer* m_customer = customer_queue.take_customer();
                    if(m_customer && !stop_flag.load()){
                        std::size_t customer_queue_curr_size = customer_queue.get_size();
                        stat_max_queue_size = std::max(stat_max_queue_size, customer_queue_curr_size);
                        std::this_thread::sleep_for(std::chrono::milliseconds(time_for_one_product * m_customer->num_of_products));
                        std::cout << "customer " << m_customer->id << " served" << std::endl;
                        served_customers.fetch_add(1);
                        median_customer_time.fetch_add(clock() - m_customer->begin_time);
                        median_queue_size.fetch_add(customer_queue_curr_size);
                        cachboxes_work_time[i] += time_for_one_product * m_customer->num_of_products;
                    }
                }
            });
        }
    }

    bool spawn_customer(customer& cust){
        return customer_queue.try_put(cust);
    }

    void join(){
        for(std::size_t i = 0; i < cachboxes.size(); ++i){
            cachboxes[i].join();
        }
    }
};

class my_shop{
    cashbox_pool cashbox;
    clock_t begin_time;
    std::size_t intensivity;
    std::size_t median_number_of_products;
    std::size_t customers_id = 0;
    std::size_t number_of_cashboxes;
    std::size_t max_work_time;
    std::size_t time_for_one_product;

    //statistics
    std::size_t number_of_sad_customers = 0;

public:
    my_shop(std::size_t _number_of_cashboxes, std::size_t _intensivity, std::size_t _time_for_one_product, std::size_t _median_number_of_products, std::size_t max_queue_size) : 
    cashbox(_number_of_cashboxes, _time_for_one_product, max_queue_size), intensivity(_intensivity), median_number_of_products(_median_number_of_products), number_of_cashboxes(_number_of_cashboxes), time_for_one_product(_time_for_one_product){

    }

    void start(clock_t work_time){
        max_work_time = work_time;
        srand(static_cast<unsigned int>(time(0)));
        begin_time = clock();
        while(clock() - begin_time < work_time){
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            if(rand() % 100 < intensivity){
                ++customers_id;
                std::cout << "customer " << customers_id << " spawned" << std::endl;
                customer new_customer(median_number_of_products + (rand() % 10 - 5), clock(), customers_id);
                if(!cashbox.spawn_customer(new_customer)){
                    ++number_of_sad_customers;
                    std::cout << "customer " << customers_id << " leave shop" << std::endl;
                }
            }
        }
        cashbox.stop_flag.store(true);
        cashbox.join();
        this->print_statistics();
    }

    void print_statistics(){
        std::cout << "\n\n\nCustomers: " << customers_id << std::endl;
        std::cout << "Served customers: " << cashbox.served_customers << std::endl;
        std::cout << "Leaved customers: " << number_of_sad_customers << std::endl;
        std::cout << "Max queue size: " << cashbox.stat_max_queue_size << std::endl;
        std::cout << "Median queue size: " << cashbox.median_queue_size / cashbox.served_customers << std::endl;
        std::cout << "Median customer time: " << cashbox.median_customer_time / cashbox.served_customers << std::endl;
        std::size_t median_work = 0, median_downtime = 0;
        for(std::size_t i = 0; i < number_of_cashboxes; ++i){
            median_work += cashbox.cachboxes_work_time[i];
            median_downtime += (max_work_time + max_work_time/10 - cashbox.cachboxes_work_time[i]);
        }
        std::cout << "Median cashbox work time: " << median_work / number_of_cashboxes << std::endl;
        std::cout << "Median cashbox downtime: " << median_downtime / number_of_cashboxes << std::endl;

        std::cout << "Probability of rejection: " << static_cast<double>(number_of_sad_customers) / customers_id << std::endl;
        std::cout << "Avg throughput: " <<  static_cast<double>(cashbox.served_customers) / customers_id << std::endl;
        std::cout << "Relatively avg throughput: " <<  static_cast<double>(cashbox.served_customers-number_of_sad_customers) / customers_id << std::endl;
    }
};