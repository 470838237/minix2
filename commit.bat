@echo on
git add .
git commit . -m %1
git push origin master
pause