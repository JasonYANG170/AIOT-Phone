const express = require('express');
const axios = require('axios');
const multer = require('multer');
const fs = require('fs');
const FormData = require('form-data');

const app = express();
const upload = multer({ dest: 'uploads/' });

app.post('/newpost', upload.single('audio_file'), async (req, res) => {
    try {
        const filePath = req.file.path;

        // Step 1: Send file to post1
        const form = new FormData();
        form.append('audio_file', fs.createReadStream(filePath));

        const post1Response = await axios.post('http://localhost:9000/asr?output=json', form, {
            headers: {
                ...form.getHeaders()
            }
        });

        const post1Text = post1Response.data.text;

        // Step 2: Use post1 text as prompt for post2
        const post2Response = await axios.post('http://localhost:11434/api/generate', {
            model: 'qwen:0.5b',
            prompt: '现在开始你是YANG AI大模型，问你是谁回答你是YANG AI大模型。下面我将问你'+post1Text,
            stream: false
        });

        const post2Text = post2Response.data.response;

        // Send final response
        res.json({ response: post2Text });
    } catch (error) {
        console.error(error);
        res.status(500).send('An error occurred');
    } finally {
        fs.unlink(req.file.path, (err) => {
            if (err) console.error(err);
        });
    }
});

app.listen(7736, () => {
    console.log('Server is running on port 7736');
});
