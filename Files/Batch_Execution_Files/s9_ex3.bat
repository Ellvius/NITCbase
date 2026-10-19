OPEN TABLE QuizMarks
SELECT * FROM QuizMarks INTO CGPAQualified WHERE cgpa > 8
OPEN TABLE CGPAQualified
SELECT * FROM CGPAQualified INTO Qualified WHERE quizMarks > 85
CLOSE TABLE CGPAQualified
DROP TABLE CGPAQualified
OPEN TABLE Qualified
SELECT rollNo,name FROM Qualified INTO KeralaQual WHERE location = Kerala
SELECT rollNo,name FROM Qualified INTO GujaratQual WHERE location = Gujarat
SELECT rollNo,name FROM Qualified INTO TelangQual WHERE location = Telangana
CLOSE TABLE Qualified
DROP TABLE Qualified