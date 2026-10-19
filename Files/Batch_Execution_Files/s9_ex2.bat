echo ----Copy each query individually from the batch file and run----
echo -TODO: Compare the result with the output from batch file
echo
SELECT * FROM Toys INTO ToysForSale WHERE stock > ten;  # Error: Mismatch in attribute type
SELECT * FROM Toys INTO Toys WHERE stock > 10;          # Error: Relation already exists
SELECT id,name,size FROM Toys INTO ToySizes;            # Error: Attribute does not exist