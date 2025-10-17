#pragma once

void initFS();
void printFile(const char* filename);
void loadJobList(const char* jobsfile);
void saveJobList(const char* jobsfile);
void deleteJobList(const char* jobsfile);