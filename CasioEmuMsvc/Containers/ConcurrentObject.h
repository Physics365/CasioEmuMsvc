#pragma once
#include <mutex>
#include <thread>
template <class T>
class ConcurrentObject {
protected:
	mutable std::mutex mtx;
	mutable std::thread::id own_thread;
	T* storage;

public:
	ConcurrentObject() {
		storage = new T();
	}
	ConcurrentObject(const T& obj) {
		storage = new T(obj);
	}
	ConcurrentObject(T&& obj) {
		storage = &obj;
	}
	~ConcurrentObject() {
		delete storage;
	}
	class ObjectRef {
	public:
		ConcurrentObject<T>& obj;
		bool own_lock = true;
		ObjectRef(ConcurrentObject<T>& obj) : obj(obj) {
			if (obj.own_thread == std::this_thread::get_id()) {
				own_lock = false;
				return;
			}
			obj.own_thread = std::this_thread::get_id();
			obj.mtx.lock();
		}
		~ObjectRef() {
			if (own_lock) {
				obj.mtx.unlock();
				obj.own_thread = {};
			}
		}
		T* operator->() {
			return obj.storage;
		}
		T& operator*() {
			return *obj.storage;
		}
	};
	class ConstObjectRef {
	public:
		const ConcurrentObject<T>& obj;
		bool own_lock = true;
		ConstObjectRef(const ConcurrentObject<T>& obj) : obj(obj) {
			if (obj.own_thread == std::this_thread::get_id()) {
				own_lock = false;
				return;
			}
			obj.own_thread = std::this_thread::get_id();
			obj.mtx.lock();
		}
		~ConstObjectRef() {
			if (own_lock) {
				obj.mtx.unlock();
				obj.own_thread ={};
			}
		}
		const T* operator->() {
			return obj.storage;
		}
		const T& operator*() {
			return *obj.storage;
		}
	};
	auto get() {
		return ObjectRef{*this};
	}
	auto get_const() const {
		return ConstObjectRef{*this};
	}
};