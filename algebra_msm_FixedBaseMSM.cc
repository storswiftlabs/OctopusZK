#include<iostream>
#include<stdexcept>
#include<unistd.h>
#include<cstring>
#include <bitset>
#include <vector>
#include "algebra_msm_FixedBaseMSM.h"
#include "BigInteger.h"


// ScopedJavaLocalRef<jbyteArray> ToJavaByteArray(JNIEnv* env,
//                                               const uint8_t* bytes,
//                                               size_t len) {
//   jbyteArray byte_array = env->NewByteArray(len);
//   CheckException(env);
//   DCHECK(byte_array);
//   env->SetByteArrayRegion(byte_array, 0, len,
//                           reinterpret_cast<const jbyte*>(bytes));
//   CheckException(env);
//   return ScopedJavaLocalRef<jbyteArray>(env, byte_array);
// }

// /**
// * JNI method calling from JAVA 
// */
// JNIEXPORT jobjectArray JNICALL Java_com_test_Test_getStudentDetails( JNIEnv *env, jclass cls)
// {
//     jobjectArray jPosRecArray = env->NewObjectArray(searchRecordResult.size(), jniPosRec->cls, NULL);
 
//     for (size_t i = 0; i < searchRecordResult.size(); i++) {
//         jobject jPosRec = env->NewObject(jniPosRec->cls, jniPosRec->constructortorID);
//         FillStudentRecValuesToJni(env, jPosRec, searchRecordResult[i]);
//         env->SetObjectArrayElement(jPosRecArray, i, jPosRec);
//     }
 
//     return jPosRecArray;
// }

/*
 * Class:     algebra_msm_FixedBaseMSM
 * Method:    serialMSMNativeHelper
 * Signature: (IILjava/util/List;Ljava/math/BigInteger;)Lalgebra/groups/AbstractGroup;
 */
using namespace std;
JNIEXPORT jbyteArray JNICALL Java_algebra_msm_FixedBaseMSM_batchMSMNativeHelper
  (JNIEnv *env, jclass obj, jint outerc, jint windowSize, jobject multiplesOfBase, jobject bigScalars)
{

  jclass java_util_ArrayList      = static_cast<jclass>(env->NewGlobalRef(env->FindClass("java/util/ArrayList")));
  jmethodID java_util_ArrayList_size = env->GetMethodID(java_util_ArrayList, "size", "()I");
  jmethodID java_util_ArrayList_get  = env->GetMethodID(java_util_ArrayList, "get", "(I)Ljava/lang/Object;");

  jint out_len = env->CallIntMethod(multiplesOfBase, java_util_ArrayList_size);
  jint inner_len = env->CallIntMethod(env->CallObjectMethod(multiplesOfBase, java_util_ArrayList_get, 0), java_util_ArrayList_size);
  
  jint batch_size = env->CallIntMethod(bigScalars, java_util_ArrayList_size);
  cout << "cpp side batch size: " << batch_size << endl;
  // jclass biginteger = env->GetObjectClass(bigScalar);

  vector<BigInt> bigScalarArray = vector<BigInt>(batch_size, BigInt());
  for(int j = 0; j < batch_size; j++){
      bigScalarArray[j] = BigInt();
  }
  vector<vector<BigInt>> multiplesOfBasePtrArray = vector<vector<BigInt>>(out_len, vector<BigInt>(inner_len, BigInt()));
  for(int i = 0; i < out_len;i++){
    for(int j = 0; j < inner_len; j++){
      multiplesOfBasePtrArray[i][j] = BigInt();
    }
  }

  for(int i =0; i < batch_size; i++){
      jbyteArray element = (jbyteArray)env->CallObjectMethod(bigScalars, java_util_ArrayList_get, i);
      char* bytes = (char*)env->GetByteArrayElements(element, NULL);
      bigScalarArray[i].len = env->GetArrayLength(element);

      memcpy(bigScalarArray[i].bytes + BigInt::capacity - bigScalarArray[i].len, 
                                bytes,
                                bigScalarArray[i].len);
      //bigScalarArray[i].print();
  }

  cout <<"multiplesOfBase size " << out_len << " " <<inner_len <<endl;

  //TODO lianke parallelize it
  for(int i = 0; i < out_len;i++){
    for(int j = 0; j < inner_len; j++){
      jbyteArray element = (jbyteArray)env->CallObjectMethod(env->CallObjectMethod(multiplesOfBase, java_util_ArrayList_get, i), java_util_ArrayList_get, j);
      char* bytes = (char*)env->GetByteArrayElements(element, NULL);
      multiplesOfBasePtrArray[i][j].len = env->GetArrayLength(element);
      memcpy(multiplesOfBasePtrArray[i][j].bytes + BigInt::capacity - multiplesOfBasePtrArray[i][j].len, bytes,  multiplesOfBasePtrArray[i][j].len);
      //cout << i << " " <<j  << " bytes len:" <<multiplesOfBasePtrArray[i][j].len <<endl;
      //multiplesOfBasePtrArray[i][j].print();
    }
  }

  jbyteArray resultByteArray = env->NewByteArray((jsize)BigInt::capacity * batch_size);
  for(int batch_index = 0; batch_index < batch_size; batch_index++){
    BigInt res = multiplesOfBasePtrArray[0][0];

    for (int outer = 0; outer < outerc; ++outer) {
        int inner = 0;
        //printf("2");
        for (int i = 0; i < windowSize; ++i) {
            if (bigScalarArray[batch_index].testBit(outer * windowSize + i)) { //Returns true if and only if the designated bit is set.
                inner |= 1 << i;
            }
        }
        //TODO lianke this inner for loop to update inner can be done better
        res = res + multiplesOfBasePtrArray[outer][inner];
    }    

    env->SetByteArrayRegion(resultByteArray, batch_index * BigInt::capacity , BigInt::capacity,   reinterpret_cast<const jbyte*>(res.bytes));
    //env->ReleaseByteArrayElements(resultByteArray, (jbyte*) res.bytes, batch_index * BigInt::capacity);
  }


  return resultByteArray;
}




/*
 * Class:     algebra_msm_FixedBaseMSM
 * Method:    doubleBatchMSMNativeHelper
 * Signature: (IIIILjava/util/ArrayList;Ljava/util/ArrayList;Ljava/util/ArrayList;)Lalgebra/groups/AbstractGroup;
 */
JNIEXPORT jobject JNICALL Java_algebra_msm_FixedBaseMSM_doubleBatchMSMNativeHelper
  (JNIEnv * env, jclass obj, jint outerc1, jint windowSize1, jint outerc2, jint windowSize2, jobject multiplesOfBase1, jobject multiplesOfBase2, jobject bigScalars){

  jclass java_util_ArrayList      = static_cast<jclass>(env->NewGlobalRef(env->FindClass("java/util/ArrayList")));
  jmethodID java_util_ArrayList_size = env->GetMethodID(java_util_ArrayList, "size", "()I");
  jmethodID java_util_ArrayList_get  = env->GetMethodID(java_util_ArrayList, "get", "(I)Ljava/lang/Object;");

  jint out_len1 = env->CallIntMethod(multiplesOfBase1, java_util_ArrayList_size);
  jint inner_len1 = env->CallIntMethod(env->CallObjectMethod(multiplesOfBase1, java_util_ArrayList_get, 0), java_util_ArrayList_size);
  jint out_len2 = env->CallIntMethod(multiplesOfBase2, java_util_ArrayList_size);
  jint inner_len2 = env->CallIntMethod(env->CallObjectMethod(multiplesOfBase2, java_util_ArrayList_get, 0), java_util_ArrayList_size);
  
  jint batch_size = env->CallIntMethod(bigScalars, java_util_ArrayList_size);
  cout << "cpp side batch size: " << batch_size << endl;

  vector<BigInt> bigScalarArray = vector<BigInt>(batch_size, BigInt());
  for(int i =0; i < batch_size; i++){
      jbyteArray element = (jbyteArray)env->CallObjectMethod(bigScalars, java_util_ArrayList_get, i);
      bigScalarArray[i].len = env->GetArrayLength(element);
      memcpy(bigScalarArray[i].bytes + BigInt::capacity - bigScalarArray[i].len, (char*)env->GetByteArrayElements(element, NULL), bigScalarArray[i].len);
  }

  //jobject first_element = env->CallObjectMethod(env->CallObjectMethod(multiplesOfBase1, java_util_ArrayList_get, 0), java_util_ArrayList_get, 0);

  cout <<"multiplesOfBase1 size " << out_len1 << " " <<inner_len1 <<endl;
  cout <<"multiplesOfBase2 size " << out_len2 << " " <<inner_len2 <<endl;

  vector<vector<BigInt>> multiplesOfBasePtrArray1 = vector<vector<BigInt>>(out_len1, vector<BigInt>(inner_len1, BigInt()));
  vector<vector<BigInt>> multiplesOfBasePtrArray2 = vector<vector<BigInt>>(out_len2, vector<BigInt>(inner_len2, BigInt()));

  //TODO parallelize it
  for(int i = 0; i < out_len1;i++){
    for(int j = 0; j < inner_len1; j++){
      jbyteArray element = (jbyteArray)env->CallObjectMethod(env->CallObjectMethod(multiplesOfBase1, java_util_ArrayList_get, i), java_util_ArrayList_get, j);
      char* bytes = (char*)env->GetByteArrayElements(element, NULL);
      multiplesOfBasePtrArray1[i][j].len = env->GetArrayLength(element);
      memcpy(multiplesOfBasePtrArray1[i][j].bytes + BigInt::capacity - multiplesOfBasePtrArray1[i][j].len, bytes,  multiplesOfBasePtrArray1[i][j].len);
    }
  }

  for(int i = 0; i < out_len2;i++){
    for(int j = 0; j < inner_len2; j++){
      jbyteArray element = (jbyteArray)env->CallObjectMethod(env->CallObjectMethod(multiplesOfBase2, java_util_ArrayList_get, i), java_util_ArrayList_get, j);
      char* bytes = (char*)env->GetByteArrayElements(element, NULL);
      multiplesOfBasePtrArray2[i][j].len = env->GetArrayLength(element);
      memcpy(multiplesOfBasePtrArray2[i][j].bytes + BigInt::capacity - multiplesOfBasePtrArray2[i][j].len, bytes,  multiplesOfBasePtrArray2[i][j].len);
    }
  }

  for(int batch_index = 0; batch_index < batch_size; batch_index++){
    BigInt res = multiplesOfBasePtrArray1[0][0];
    for (int outer = 0; outer < outerc1; ++outer) {
        int inner = 0;
        for (int i = 0; i < windowSize1; ++i) {
            if (bigScalarArray[batch_index].testBit(outer * windowSize1 + i)) { //Returns true if and only if the designated bit is set.
                inner |= 1 << i;
            }
        }
        res = res + multiplesOfBasePtrArray1[outer][inner];
    }

    for (int outer = 0; outer < outerc2; ++outer) {
        int inner = 0;
        for (int i = 0; i < windowSize2; ++i) {
            if (bigScalarArray[batch_index].testBit(outer * windowSize2 + i)) { //Returns true if and only if the designated bit is set.
                inner |= 1 << i;
            }
        }
        res = res + multiplesOfBasePtrArray2[outer][inner];
    }
  }


  //TODO add return value
  return bigScalars;

  }





//lianke : below are old codebases

  // for(int i=0; i < lengthOfArray;i++){
  //   for(int j = 0; j < 8; j++){
  //     cout <<bigScalarBitArray[i][j] << " ";
  //   }
  // }

  // jfieldID fp_fid = env->GetFieldID(cls, "element", "Lalgebra/fields/Fp;");
  // jobject fp = env->GetObjectField(first_element, fp_fid);

  // jclass fp_cls = env->GetObjectClass(fp);
  // jfieldID biginteger_fid = env->GetFieldID(fp_cls, "number", "Ljava/math/BigInteger;");
  
    // vector<bitset<8>> bigScalarBitArray;
  // for(int i=0; i < lengthOfArray;i++){
  //   // std::bitset<8> arr(bufferPtr[lengthOfArray -1 -i]);
  //   // bigScalarBitArray.push_back(arr);
  //   printf("%hhx |", bufferPtr[i]);
  // }
  //this arr[i] equals to BigInteger.testBit(i)