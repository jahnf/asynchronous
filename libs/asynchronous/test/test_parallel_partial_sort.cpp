
// Boost.Asynchronous library
//  Copyright (C) Christophe Henry 2013
//
//  Use, modification and distribution is subject to the Boost
//  Software License, Version 1.0.  (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// For more information, see http://www.boost.org

#include <vector>
#include <set>
#include <functional>
#include <random>
#include <future>

#include <boost/lexical_cast.hpp>

#include <boost/asynchronous/scheduler/single_thread_scheduler.hpp>
#include <boost/asynchronous/queue/lockfree_queue.hpp>
#include <boost/asynchronous/scheduler_shared_proxy.hpp>
#include <boost/asynchronous/scheduler/threadpool_scheduler.hpp>

#include <boost/asynchronous/servant_proxy.hpp>
#include <boost/asynchronous/post.hpp>
#include <boost/asynchronous/trackable_servant.hpp>
#include <boost/asynchronous/algorithm/parallel_partial_sort.hpp>

#include "test_common.hpp"

#include <boost/test/unit_test.hpp>
using namespace boost::asynchronous::test;

namespace
{
// main thread id
boost::thread::id main_thread_id;
bool servant_dtor=false;
typedef std::vector<int>::iterator Iterator;
typedef std::vector<int>::const_iterator ConstIterator;

void generate(std::vector<int>& data, unsigned elements, unsigned dist)
{
    data = std::vector<int>(elements,1);
    std::random_device rd;
    std::mt19937 mt(rd());
    //std::mt19937 mt(static_cast<unsigned int>(std::time(nullptr)));
    std::uniform_int_distribution<> dis(0, dist);
    std::generate(data.begin(), data.end(), std::bind(dis, std::ref(mt)));
}

struct Servant : boost::asynchronous::trackable_servant<>
{
    typedef int simple_ctor;
    Servant(boost::asynchronous::any_weak_scheduler<> scheduler)
        : boost::asynchronous::trackable_servant<>(scheduler,
                                               boost::asynchronous::create_shared_scheduler_proxy(
                                                   new boost::asynchronous::threadpool_scheduler<
                                                           boost::asynchronous::lockfree_queue<> >(boost::thread::hardware_concurrency())))
    {
    }
    ~Servant()
    {
        BOOST_CHECK_MESSAGE(main_thread_id!=boost::this_thread::get_id(),"servant dtor not posted.");
        servant_dtor = true;
    }

    std::future<void> test_parallel_partial_sort_it_small()
    {
        BOOST_CHECK_MESSAGE(main_thread_id!=boost::this_thread::get_id(),"servant async work not posted.");
        generate(m_data1,100000,90000);
        auto data_copy = m_data1;
        // we need a promise to inform caller when we're done
        std::shared_ptr<std::promise<void> > aPromise(new std::promise<void>);
        std::future<void> fu = aPromise->get_future();
        boost::asynchronous::any_shared_scheduler_proxy<> tp =get_worker();
        std::vector<boost::thread::id> ids = tp.thread_ids();

        auto middle = m_data1.begin()+20000;

        // start long tasks
        post_callback(
           [ids,this,middle](){
                    BOOST_CHECK_MESSAGE(main_thread_id!=boost::this_thread::get_id(),"servant work not posted.");
                    BOOST_CHECK_MESSAGE(contains_id(ids.begin(),ids.end(),boost::this_thread::get_id()),"task executed in the wrong thread");
                    return boost::asynchronous::parallel_partial_sort(m_data1.begin(),middle,m_data1.end(),
                                                                     [](int i,int j){return i < j;},
                                                                     2000,
                                                                     boost::thread::hardware_concurrency()
                    );
                    },// work
           [aPromise,ids,data_copy,this,middle](boost::asynchronous::expected<void> res) mutable{
                        BOOST_CHECK_MESSAGE(!res.has_exception(),"servant work threw an exception.");
                        BOOST_CHECK_MESSAGE(main_thread_id!=boost::this_thread::get_id(),"servant callback in main thread.");
                        BOOST_CHECK_MESSAGE(!contains_id(ids.begin(),ids.end(),boost::this_thread::get_id()),"task callback executed in the wrong thread(pool)");
                        BOOST_CHECK_MESSAGE(!res.has_exception(),"servant work threw an exception.");

                        auto middle2 = data_copy.begin()+20000;
                        std::partial_sort(data_copy.begin(),middle2,data_copy.end(),
                                         [](int i,int j){return i < j;});
                        BOOST_CHECK_MESSAGE(std::is_sorted(m_data1.begin(),middle),"parallel_partial_sort did not sort.");
                        bool sort_error = false;
                        for (auto it = middle+1; it != m_data1.end(); ++it)
                        {
                            if (*it < *middle)
                                sort_error=true;
                        }
                        BOOST_CHECK_MESSAGE(!sort_error,"parallel_partial_sort did not sort correctly.");
                        BOOST_CHECK_MESSAGE(std::equal(m_data1.begin(),middle,data_copy.begin()),"parallel_partial_sort gave the wrong result.");
                        aPromise->set_value();
           }// callback functor.
        );
        return fu;
    }
    std::future<void> test_parallel_partial_sort_it_big()
    {
        BOOST_CHECK_MESSAGE(main_thread_id!=boost::this_thread::get_id(),"servant async work not posted.");
        generate(m_data1,100000,90000);
        auto data_copy = m_data1;
        // we need a promise to inform caller when we're done
        std::shared_ptr<std::promise<void> > aPromise(new std::promise<void>);
        std::future<void> fu = aPromise->get_future();
        boost::asynchronous::any_shared_scheduler_proxy<> tp =get_worker();
        std::vector<boost::thread::id> ids = tp.thread_ids();

        auto middle = m_data1.begin()+80000;

        // start long tasks
        post_callback(
           [ids,this,middle](){
                    BOOST_CHECK_MESSAGE(main_thread_id!=boost::this_thread::get_id(),"servant work not posted.");
                    BOOST_CHECK_MESSAGE(contains_id(ids.begin(),ids.end(),boost::this_thread::get_id()),"task executed in the wrong thread");
                    return boost::asynchronous::parallel_partial_sort(m_data1.begin(),middle,m_data1.end(),
                                                                     [](int i,int j){return i < j;},
                                                                     2000,
                                                                     boost::thread::hardware_concurrency()
                    );
                    },// work
           [aPromise,ids,data_copy,this,middle](boost::asynchronous::expected<void> res) mutable{
                        BOOST_CHECK_MESSAGE(!res.has_exception(),"servant work threw an exception.");
                        BOOST_CHECK_MESSAGE(main_thread_id!=boost::this_thread::get_id(),"servant callback in main thread.");
                        BOOST_CHECK_MESSAGE(!contains_id(ids.begin(),ids.end(),boost::this_thread::get_id()),"task callback executed in the wrong thread(pool)");
                        BOOST_CHECK_MESSAGE(!res.has_exception(),"servant work threw an exception.");

                        auto middle2 = data_copy.begin()+80000;
                        std::partial_sort(data_copy.begin(),middle2,data_copy.end(),
                                         [](int i,int j){return i < j;});
                        BOOST_CHECK_MESSAGE(std::is_sorted(m_data1.begin(),middle),"parallel_partial_sort did not sort.");
                        bool sort_error = false;
                        for (auto it = middle+1; it != m_data1.end(); ++it)
                        {
                            if (*it < *middle)
                                sort_error=true;
                        }
                        BOOST_CHECK_MESSAGE(!sort_error,"parallel_partial_sort did not sort correctly.");
                        BOOST_CHECK_MESSAGE(std::equal(m_data1.begin(),middle,data_copy.begin()),"parallel_partial_sort gave the wrong result.");
                        aPromise->set_value();
           }// callback functor.
        );
        return fu;
    }
    std::future<void> test_parallel_partial_sort_it_dense()
    {
        BOOST_CHECK_MESSAGE(main_thread_id!=boost::this_thread::get_id(),"servant async work not posted.");
        generate(m_data1,100000,20000);
        auto data_copy = m_data1;
        // we need a promise to inform caller when we're done
        std::shared_ptr<std::promise<void> > aPromise(new std::promise<void>);
        std::future<void> fu = aPromise->get_future();
        boost::asynchronous::any_shared_scheduler_proxy<> tp =get_worker();
        std::vector<boost::thread::id> ids = tp.thread_ids();

        auto middle = m_data1.begin()+20000;

        // start long tasks
        post_callback(
           [ids,this,middle](){
                    BOOST_CHECK_MESSAGE(main_thread_id!=boost::this_thread::get_id(),"servant work not posted.");
                    BOOST_CHECK_MESSAGE(contains_id(ids.begin(),ids.end(),boost::this_thread::get_id()),"task executed in the wrong thread");
                    return boost::asynchronous::parallel_partial_sort(m_data1.begin(),middle,m_data1.end(),
                                                                     [](int i,int j){return i < j;},
                                                                     2000,
                                                                     boost::thread::hardware_concurrency()
                    );
                    },// work
           [aPromise,ids,data_copy,this,middle](boost::asynchronous::expected<void> res) mutable{
                        BOOST_CHECK_MESSAGE(!res.has_exception(),"servant work threw an exception.");
                        BOOST_CHECK_MESSAGE(main_thread_id!=boost::this_thread::get_id(),"servant callback in main thread.");
                        BOOST_CHECK_MESSAGE(!contains_id(ids.begin(),ids.end(),boost::this_thread::get_id()),"task callback executed in the wrong thread(pool)");
                        BOOST_CHECK_MESSAGE(!res.has_exception(),"servant work threw an exception.");

                        auto middle2 = data_copy.begin()+20000;
                        std::partial_sort(data_copy.begin(),middle2,data_copy.end(),
                                         [](int i,int j){return i < j;});
                        BOOST_CHECK_MESSAGE(std::is_sorted(m_data1.begin(),middle),"parallel_partial_sort did not sort.");
                        bool sort_error = false;
                        for (auto it = middle+1; it != m_data1.end(); ++it)
                        {
                            if (*it < *middle)
                                sort_error=true;
                        }
                        BOOST_CHECK_MESSAGE(!sort_error,"parallel_partial_sort did not sort correctly.");
                        BOOST_CHECK_MESSAGE(std::equal(m_data1.begin(),middle,data_copy.begin()),"parallel_partial_sort gave the wrong result.");
                        aPromise->set_value();
           }// callback functor.
        );
        return fu;
    }
private:
    std::vector<int> m_data1;
};
class ServantProxy : public boost::asynchronous::servant_proxy<ServantProxy,Servant>
{
public:
    template <class Scheduler>
    ServantProxy(Scheduler s):
        boost::asynchronous::servant_proxy<ServantProxy,Servant>(s)
    {}
    BOOST_ASYNC_FUTURE_MEMBER(test_parallel_partial_sort_it_small)
    BOOST_ASYNC_FUTURE_MEMBER(test_parallel_partial_sort_it_big)
    BOOST_ASYNC_FUTURE_MEMBER(test_parallel_partial_sort_it_dense)
};

}

BOOST_AUTO_TEST_CASE( test_parallel_partial_sort_it_small)
{
    servant_dtor=false;
    {
        auto scheduler = boost::asynchronous::create_shared_scheduler_proxy(new boost::asynchronous::single_thread_scheduler<
                                                                            boost::asynchronous::lockfree_queue<> >);

        main_thread_id = boost::this_thread::get_id();
        ServantProxy proxy(scheduler);
        auto fuv = proxy.test_parallel_partial_sort_it_small();
        try
        {
            auto resfuv = fuv.get();
            resfuv.get();
        }
        catch(...)
        {
            BOOST_FAIL( "unexpected exception" );
        }
    }
    BOOST_CHECK_MESSAGE(servant_dtor,"servant dtor not called.");
}
BOOST_AUTO_TEST_CASE( test_parallel_partial_sort_it_big)
{
    servant_dtor=false;
    {
        auto scheduler = boost::asynchronous::create_shared_scheduler_proxy(new boost::asynchronous::single_thread_scheduler<
                                                                            boost::asynchronous::lockfree_queue<> >);

        main_thread_id = boost::this_thread::get_id();
        ServantProxy proxy(scheduler);
        auto fuv = proxy.test_parallel_partial_sort_it_big();
        try
        {
            auto resfuv = fuv.get();
            resfuv.get();
        }
        catch(...)
        {
            BOOST_FAIL( "unexpected exception" );
        }
    }
    BOOST_CHECK_MESSAGE(servant_dtor,"servant dtor not called.");
}
BOOST_AUTO_TEST_CASE( test_parallel_partial_sort_it_dense)
{
    servant_dtor=false;
    {
        auto scheduler = boost::asynchronous::create_shared_scheduler_proxy(new boost::asynchronous::single_thread_scheduler<
                                                                            boost::asynchronous::lockfree_queue<> >);

        main_thread_id = boost::this_thread::get_id();
        ServantProxy proxy(scheduler);
        auto fuv = proxy.test_parallel_partial_sort_it_dense();
        try
        {
            auto resfuv = fuv.get();
            resfuv.get();
        }
        catch(...)
        {
            BOOST_FAIL( "unexpected exception" );
        }
    }
    BOOST_CHECK_MESSAGE(servant_dtor,"servant dtor not called.");
}
