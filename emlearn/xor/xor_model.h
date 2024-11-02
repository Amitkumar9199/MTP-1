


    // !!! This file is generated using emlearn !!!

    #include <eml_trees.h>
    

static const EmlTreesNode xor_model_nodes[13] = {
  { 1, 49, 1, 2 },
  { 0, 118, -1, -2 },
  { 1, 65, 1, 2 },
  { 0, 118, -1, -2 },
  { 0, 121, -2, -1 },
  { 1, 229, 1, -2 },
  { 0, 128, 1, 2 },
  { 1, 127, -1, -2 },
  { 1, 126, -2, -1 },
  { 0, 11, -2, 1 },
  { 1, 66, 1, 2 },
  { 0, 120, -1, -2 },
  { 0, 107, -2, -1 } 
};

static const int32_t xor_model_tree_roots[3] = { 0, 5, 9 };

static const uint8_t xor_model_leaves[2] = { 0, 1 };

EmlTrees xor_model = {
        13,
        (EmlTreesNode *)(xor_model_nodes),	  
        3,
        (int32_t *)(xor_model_tree_roots),
        2,
        (uint8_t *)(xor_model_leaves),
        0,
        2,
        2,
    };

static inline int32_t xor_model_tree_0(const int16_t *features, int32_t features_length) {
          if (features[1] < 49) {
              if (features[0] < 118) {
                  return 0;
              } else {
                  return 1;
              }
          } else {
              if (features[1] < 65) {
                  if (features[0] < 118) {
                      return 0;
                  } else {
                      return 1;
                  }
              } else {
                  if (features[0] < 121) {
                      return 1;
                  } else {
                      return 0;
                  }
              }
          }
        }
        

static inline int32_t xor_model_tree_1(const int16_t *features, int32_t features_length) {
          if (features[1] < 229) {
              if (features[0] < 128) {
                  if (features[1] < 127) {
                      return 0;
                  } else {
                      return 1;
                  }
              } else {
                  if (features[1] < 126) {
                      return 1;
                  } else {
                      return 0;
                  }
              }
          } else {
              return 1;
          }
        }
        

static inline int32_t xor_model_tree_2(const int16_t *features, int32_t features_length) {
          if (features[0] < 11) {
              return 1;
          } else {
              if (features[1] < 66) {
                  if (features[0] < 120) {
                      return 0;
                  } else {
                      return 1;
                  }
              } else {
                  if (features[0] < 107) {
                      return 1;
                  } else {
                      return 0;
                  }
              }
          }
        }
        

int32_t xor_model_predict(const int16_t *features, int32_t features_length) {

        int32_t votes[2] = {0,};
        int32_t _class = -1;

        _class = xor_model_tree_0(features, features_length); votes[_class] += 1;
    _class = xor_model_tree_1(features, features_length); votes[_class] += 1;
    _class = xor_model_tree_2(features, features_length); votes[_class] += 1;
    
        int32_t most_voted_class = -1;
        int32_t most_voted_votes = 0;
        for (int32_t i=0; i<2; i++) {

            if (votes[i] > most_voted_votes) {
                most_voted_class = i;
                most_voted_votes = votes[i];
            }
        }
        return most_voted_class;
    }
    