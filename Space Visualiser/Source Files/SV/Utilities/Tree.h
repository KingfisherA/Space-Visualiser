#pragma once
#include"Array.hpp"

namespace SV
{
	template<typename A>class HNode;

	template<typename A> class HNode
	{
	public:

		HNode() {}
		HNode(const A& dataIn) : data(dataIn) {}
		HNode(A&& dataIn) : data(std::move(dataIn)) {}
		HNode(const HNode<A>& node) : data(node.data) {}
		HNode(HNode<A>&& node) : data(std::move(node.data)) {}

		virtual ~HNode()
		{
			for (HNode* ptr : children)
			{
				delete ptr;
			}
		}

		HNode<A>& operator=(const HNode<A>& node)
		{
			this->data = node.data;
			return *this;
		}
		HNode<A>& operator=(HNode<A>&& node)
		{
			this->data = std::move(node.data);
			return *this;
		}
		HNode<A>& operator=(const A& dataIn)
		{
			this->data = dataIn;
			return *this;
		}
		HNode<A>& operator=(A&& dataIn)
		{
			this->data = std::move(dataIn);
			return *this;
		}
		//
		size_t Add(HNode<A>* ptr)
		{
			ptr->parent = this;
			return children.Add(ptr);
		}
		//
		HNode<A>*& operator[](size_t index)
		{
			return children[index];
		}
		const HNode<A>* operator[](size_t index) const
		{
			return children[index];
		}
		HNode<A>*& Get(size_t index)
		{
			return children[index];
		}
		const HNode<A>* Get(size_t index) const
		{
			return children[index];
		}
		const HNode<A>* GetParent() const
		{
			return parent;
		}
		//
		void Dissociate(const size_t index)
		{
			SV_ASSERT_T(!children.Exists(index), SV::HNode<A>, "Index inputted invalid");
			children[index]->parent = nullptr;
			children.Remove(index);
		}
		void Dissociate(const HNode<A>* ptr)
		{
			size_t index = children.Find(ptr);
			SV_ASSERT_T(index == IndexUndefined, SV::HNode<A>, "Index inputted invalid");
			children[index]->parent = nullptr;
			children.Remove(index);
		}
		void DissociateAll()
		{
			children.Clear();
		}
		//
		void Remove(const size_t index)
		{
			SV_ASSERT_T(!children.Exists(index), SV::HNode<A>, "Index inputted invalid");
			delete children[index];
			children.Remove(index);
		}
		void Remove(const HNode<A>* ptr)
		{
			size_t index = children.Find(ptr);
			SV_ASSERT_T(index == IndexUndefined, SV::HNode<A>, "Index inputted invalid");
			delete children[index];
			children.Remove(index);
		}
		//
		void Clear()
		{
			this->~HNode();
			children.Clear();
		}
		//
	private:
		SV::Array<HNode<A>*> children;
		HNode<A>* parent = nullptr;

		A data;
	};

}


#include"Tree.cpp"