#pragma once

// calibration ������ ��ó�� �Լ�
// https://github.com/wang-xinyu/tensorrtx/tree/master/retinaface
cv::Mat preprocess_img_cali(cv::Mat& img, int input_w, int input_h);

// Ư�� ������ ���� �̸� ����Ʈ ��� �Լ�
int read_files_in_dir(const char *p_dir_name, std::vector<std::string> &file_names);

// cpp ��ó�� �Լ� 
//void preprocessImg(cv::Mat& img, int newh, int neww);