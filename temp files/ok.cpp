#include<bits/stdc++.h>

using namespace std;

struct CompareSecond {
    bool operator()(const std::pair<std::string, int>& lhs, const std::pair<std::string, int>& rhs) const {
        if (lhs.second != rhs.second) {
            return lhs.second < rhs.second; // Compare second item (integer part)
            
        }
        return lhs.first < rhs.first; // Compare first item (string part)
    }
};

int main(){
    set<pair<string,int>,CompareSecond> st;
    st.insert({"123",1});

    // st.insert({"125",1}); // This does not gets inserted.
    
    st.insert({"456",2});

    for(auto i:st){
        cout<<i.first<<" "<<i.second<<"\n";
    }

    if(st.lower_bound({"",1})!=st.end()){
        cout<<(st.lower_bound({"",1})->first)<<"\n";
    }
    else{
        cout<<"Not Found\n";
    }
    return 0;
}