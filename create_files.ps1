$content1 = @"
25-195.geojson
25-60.geojson
60-210.geojson
66-210.geojson
66-211.geojson
67-211.geojson
67-97.geojson
96-97.geojson
96-224.geojson
94-224.geojson
85-94.geojson
85-91.geojson
91-242.geojson
242-245.geojson
245-246.geojson
"@

$content24 = @"
195-197.geojson
32-197.geojson
32-35.geojson
34-35.geojson
34-106.geojson
103-106.geojson
103-225.geojson
224-225.geojson
94-224.geojson
85-94.geojson
85-91.geojson
91-242.geojson
242-245.geojson
245-246.geojson
"@

$content85 = @"
25-195.geojson
25-60.geojson
60-210.geojson
210-212.geojson
212-67.geojson
67-97.geojson
96-97.geojson
96-224.geojson
94-224.geojson
85-94.geojson
85-91.geojson
91-242.geojson
242-245.geojson
245-246.geojson
"@

$dir = "192-194_green"

# 1-23.txt
for ($i = 1; $i -le 23; $i++) {
    $content1 | Out-File -FilePath "$dir\$i.txt" -Encoding utf8 -NoNewline
}

# 24-84.txt と 131.txt
for ($i = 24; $i -le 84; $i++) {
    $content24 | Out-File -FilePath "$dir\$i.txt" -Encoding utf8 -NoNewline
}

# 85-130.txt
for ($i = 85; $i -le 130; $i++) {
    $content85 | Out-File -FilePath "$dir\$i.txt" -Encoding utf8 -NoNewline
}

# 131.txt
$content24 | Out-File -FilePath "$dir\131.txt" -Encoding utf8 -NoNewline

